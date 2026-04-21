# 从零开始的 JSON 库教程（二）：解析数字解答篇

本文是第二个单元的解答篇。

## 1. 重构合并 parseLiteral

由于 true / false / null 的解析逻辑完全一致，只是字面值和类型不同，我们提取 `parseLiteral()`：

~~~cpp
/* literal = "null" / "true" / "false" */
ParseError Value::parseLiteral(Context& c, Value& v,
                               std::string_view literal, Type type)
{
    if (c.json.size() < literal.size() || c.json.substr(0, literal.size()) != literal)
        return ParseError::InvalidValue;
    c.json.remove_prefix(literal.size());
    v.type_ = type;
    return ParseError::OK;
}
~~~

然后在 `parseValue()` 中调用：

~~~cpp
ParseError Value::parseValue(Context& c, Value& v)
{
    char ch = c.json.empty() ? '\0' : c.json.front();
    switch (ch)
    {
    case 'n': return parseLiteral(c, v, "null", Type::Null);
    case 't': return parseLiteral(c, v, "true", Type::True);
    case 'f': return parseLiteral(c, v, "false", Type::False);
    default:  return parseNumber(c, v);
    case '\0': return ParseError::ExpectValue;
    }
}
~~~

`std::string_view` 的 `substr` 和比较操作零拷贝，合并后代码更简洁。

## 2. 边界值测试

以下是一些 IEEE-754 双精度浮点数的边界值：

~~~cpp
test_number(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
test_number( 4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
test_number(-4.9406564584124654e-324, "-4.9406564584124654e-324");
test_number( 2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
test_number(-2.2250738585072009e-308, "-2.2250738585072009e-308");
test_number( 2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
test_number(-2.2250738585072014e-308, "-2.2250738585072014e-308");
test_number( 1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
test_number(-1.7976931348623157e+308, "-1.7976931348623157e+308");
~~~

这些加入的测试用例，正常的 `std::strtod()` 都能通过。有一些 JSON 解析器不使用 `std::strtod()` 而自行转换，这需要非常仔细地处理精度问题。有兴趣的同学可以参考 Google 的 [double-conversion](https://github.com/google/double-conversion) 开源项目及相关论文。

## 3. 校验数字

这条题目是本单元的重点，考验同学是否能把语法手写为校验规则。

我们使用一个局部的 `string_view` 来表示当前的解析位置。这样做的好处是，校验失败时不需要恢复原始位置，校验成功后才更新 `c.json`：

~~~cpp
ParseError Value::parseNumber(Context& c, Value& v)
{
    std::string_view p = c.json;
    /* 负号 ... */
    /* 整数 ... */
    /* 小数 ... */
    /* 指数 ... */
    v.number_ = std::strtod(c.json.data(), nullptr);
    v.type_ = Type::Number;
    c.json = p;
    return ParseError::OK;
}
~~~

我们逐部分处理语法：

~~~
number = [ "-" ] int [ frac ] [ exp ]
int = "0" / digit1-9 *digit
frac = "." 1*digit
exp = ("e" / "E") ["-" / "+"] 1*digit
~~~

**负号**，有的话跳过：

~~~cpp
    if (!p.empty() && p.front() == '-') p.remove_prefix(1);
~~~

**整数部分**有两种合法情况：单个 `0`，或者 1-9 开头后跟任意个 digit：

~~~cpp
    if (p.empty()) return ParseError::InvalidValue;
    if (p.front() == '0')
        p.remove_prefix(1);
    else
    {
        if (!isDigit1to9(p.front())) return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }
~~~

**小数部分**，如果出现小数点，跳过小数点后至少一个 digit：

~~~cpp
    if (!p.empty() && p.front() == '.')
    {
        p.remove_prefix(1);
        if (p.empty() || !isDigit(p.front())) return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }
~~~

**指数部分**，如果出现 e/E，跳过后可选正负号，然后至少一个 digit：

~~~cpp
    if (!p.empty() && (p.front() == 'e' || p.front() == 'E'))
    {
        p.remove_prefix(1);
        if (!p.empty() && (p.front() == '+' || p.front() == '-'))
            p.remove_prefix(1);
        if (p.empty() || !isDigit(p.front())) return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }
~~~

辅助函数：

~~~cpp
static bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }
static bool isDigit1to9(char ch) { return ch >= '1' && ch <= '9'; }
~~~

使用 `string_view` 的 `empty()` 检查来替代 C 版本中对 `'\0'` 的检查，`remove_prefix()` 替代指针后移，逻辑更清晰。

## 4. 数字过大的处理

`std::strtod()` 在转换结果超出 `double` 表示范围时，会返回 `HUGE_VAL` 并设置 `errno` 为 `ERANGE`。我们据此添加新的错误码和检测：

首先在 `ParseError` 枚举中添加：

~~~cpp
enum class ParseError
{
    OK = 0,
    ExpectValue,
    InvalidValue,
    RootNotSingular,
    NumberTooBig
};
~~~

然后在 `parseNumber()` 中，校验成功后进行转换时检测：

~~~cpp
#include <cerrno>
#include <cmath>

// ...

    errno = 0;
    v.number_ = std::strtod(c.json.data(), nullptr);
    if (errno == ERANGE && (v.number_ == HUGE_VAL || v.number_ == -HUGE_VAL))
        return ParseError::NumberTooBig;
~~~

注意要同时检测正负的 `HUGE_VAL`，因为负数也可能溢出。下溢（underflow，如 `1e-10000`）时 `strtod` 也会设置 `errno == ERANGE`，但返回 0 而不是 `HUGE_VAL`，所以我们只检测 `HUGE_VAL` 来区分上溢和下溢。

许多时候课本和书籍不会把每个标准库功能说得很仔细，学会读文档编程就简单得多！[cppreference.com](https://cppreference.com) 是 C/C++ 程序员的宝库。
