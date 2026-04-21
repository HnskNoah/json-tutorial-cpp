# 从零开始的 JSON 库教程（二）：解析数字

本文是《从零开始的 JSON 库教程》的第二个单元。

本单元内容：

1. [初探重构](#1-初探重构)
2. [JSON 数字语法](#2-json-数字语法)
3. [数字表示方式](#3-数字表示方式)
4. [单元测试](#4-单元测试)
5. [十进制转换至二进制](#5-十进制转换至二进制)
6. [总结与练习](#6-总结与练习)

## 1. 初探重构

在讨论解析数字之前，我们先补充 TDD 中的一个步骤——重构（refactoring）。根据 [1]，重构是一个这样的过程：

> 在不改变代码外在行为的情况下，对代码作出修改，以改进程序的内部结构。

在 TDD 的过程中，我们的目标是编写代码去通过测试。但由于这个目标的引导性太强，我们可能会忽略正确性以外的软件品质。在通过测试之后，代码的正确性得以保证，我们就应该审视现时的代码，看看有没有地方可以改进，而同时能维持测试顺利通过。我们可以安心地做各种修改，因为我们有单元测试，可以判断代码在修改后是否影响原来的行为。

那么，哪里要作出修改？Beck 和 Fowler（[1] 第 3 章）认为程序员要培养一种判断能力，找出程序中的坏味道。例如，在第一单元中，`parseNull()`、`parseTrue()` 和 `parseFalse()` 三个函数非常相似，都是：断言首字符、跳过首字符、比较剩余字符、设置类型。这违反了 DRY（don't repeat yourself）原则。本单元的第一个练习，就是合并这 3 个函数。

我们可以提取一个通用的 `parseLiteral()` 函数：

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

`std::string_view` 作为参数可以直接接受字符串字面量（如 `"null"`），`substr` 和比较操作零拷贝。然后在 `parseValue()` 中调用：

~~~cpp
case 'n': return parseLiteral(c, v, "null", Type::Null);
case 't': return parseLiteral(c, v, "true", Type::True);
case 'f': return parseLiteral(c, v, "false", Type::False);
~~~

合并后减少了重复代码，维护更方便。缺点是可能带来少许性能影响（多传了参数），但在实际应用中几乎可以忽略。

另外，测试代码中也有重复的模式。例如 `test_parse_invalid_value()` 每次测试一个不合法的 JSON 值，都是相同的步骤。我们可以提取一个辅助函数来简化：

~~~cpp
static void test_error(ParseError error, std::string_view json)
{
    Value v;
    runner.expect_eq(error, v.parse(json));
    runner.expect_eq(Type::Null, v.type());
}

static void test_parse_expect_value()
{
    test_error(ParseError::ExpectValue, "");
    test_error(ParseError::ExpectValue, " ");
}
~~~

最后，我希望指出，软件的架构难以用单一标准评分，重构时要考虑平衡各种软件品质。

## 2. JSON 数字语法

本单元的重点在于解析 JSON number 类型。我们先看看它的语法：

~~~
number = [ "-" ] int [ frac ] [ exp ]
int = "0" / digit1-9 *digit
frac = "." 1*digit
exp = ("e" / "E") ["-" / "+"] 1*digit
~~~

number 是以十进制表示，它主要由 4 部分顺序组成：负号、整数、小数、指数。只有整数是必需部分。注意和直觉可能不同的是，正号是不合法的。

整数部分如果是 0 开始，只能是单个 0；而由 1-9 开始的话，可以加任意数量的数字（0-9）。也就是说，`0123` 不是一个合法的 JSON 数字。

小数部分比较直观，就是小数点后是一或多个数字（0-9）。

JSON 可使用科学记数法，指数部分由大写 E 或小写 e 开始，然后可有正负号，之后是一或多个数字（0-9）。

一些合法与不合法的例子：

| 输入 | 合法？ | 原因 |
|------|--------|------|
| `0` | 合法 | |
| `-0` | 合法 | |
| `1` | 合法 | |
| `0123` | 不合法 | 整数不能有前导零 |
| `+1` | 不合法 | 正号不合法 |
| `.123` | 不合法 | 小数点前至少一位数字 |
| `1.` | 不合法 | 小数点后至少一位数字 |
| `1E10` | 合法 | |
| `1e-10` | 合法 | |
| `1E012` | 合法 | 指数允许前导零 |

上一单元的 null、false、true 在解析后，我们只需把它们存储为类型。但对于数字，我们要考虑怎么存储解析后的结果。

## 3. 数字表示方式

从 JSON 数字的语法，我们可能直观地会认为它应该表示为一个浮点数（floating point number），因为它带有小数和指数部分。然而，标准中并没有限制数字的范围或精度。为简单起见，leptjson 选择以双精度浮点数（`double`）来存储 JSON 数字。

我们为 `Value` 类添加成员：

~~~cpp
class Value
{
public:
    // ...
    double number() const
    {
        assert(type_ == Type::Number);
        return number_;
    }

private:
    Type type_;
    double number_;
};
~~~

仅当 `type_ == Type::Number` 时，`number_` 才表示 JSON 数字的数值。使用者应确保类型正确，才调用此函数。我们继续使用断言来保证。

## 4. 单元测试

定义了 API 之后，按照 TDD，我们先写一些单元测试。类似上一节提取的 `test_error()`，我们也可以提取一个测试数字解析的辅助函数：

~~~cpp
static void test_number(double expect, std::string_view json)
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse(json));
    runner.expect_eq(Type::Number, v.type());
    runner.expect_eq(expect, v.number());
}
~~~

注意 `expect_eq` 的模板可以接受 `double` 类型的比较，但浮点数的直接比较可能因精度问题而失败。更严谨的做法会在后面讨论，这里我们先用简单的相等比较。

然后我们可以编写各种测试用例：

~~~cpp
static void test_parse_number()
{
    test_number(0.0, "0");
    test_number(0.0, "-0");
    test_number(0.0, "-0.0");
    test_number(1.0, "1");
    test_number(-1.0, "-1");
    test_number(1.5, "1.5");
    test_number(-1.5, "-1.5");
    test_number(3.1416, "3.1416");
    test_number(1E10, "1E10");
    test_number(1e10, "1e10");
    test_number(1E+10, "1E+10");
    test_number(1E-10, "1E-10");
    test_number(-1E10, "-1E10");
    test_number(-1e10, "-1e10");
    test_number(-1E+10, "-1E+10");
    test_number(-1E-10, "-1E-10");
    test_number(1.234E+10, "1.234E+10");
    test_number(1.234E-10, "1.234E-10");
    test_number(0.0, "1e-10000"); /* must underflow */
}
~~~

以上这些都是很基本的测试用例，也可供调试用。大部分情况下，测试案例不能穷举所有可能性。因此，除了加入一些典型的用例，我们也常会使用一些边界值，例如最大值等。练习中会让同学找一些边界值作为用例。

除了这些合法的 JSON，我们也要写一些不合语法的用例：

~~~cpp
static void test_parse_invalid_value()
{
    // ...
    /* invalid number */
    test_error(ParseError::InvalidValue, "+0");
    test_error(ParseError::InvalidValue, "+1");
    test_error(ParseError::InvalidValue, ".123"); /* at least one digit before '.' */
    test_error(ParseError::InvalidValue, "1.");   /* at least one digit after '.' */
    test_error(ParseError::InvalidValue, "INF");
    test_error(ParseError::InvalidValue, "inf");
    test_error(ParseError::InvalidValue, "NAN");
    test_error(ParseError::InvalidValue, "nan");
}
~~~

## 5. 十进制转换至二进制

我们需要把十进制的数字字符串转换成二进制的 `double`。这并不是容易的事情 [2]。为了简单起见，leptjson 使用标准库的 [`std::strtod()`](https://en.cppreference.com/w/cpp/string/byte/strtof)（需 `#include <cstdlib>`）来进行转换。`std::strtod()` 可转换 JSON 所要求的格式，但问题是，一些 JSON 不容许的格式（如 `INF`、`nan`），`std::strtod()` 也可转换，所以我们需要自行做格式校验。

~~~cpp
#include <cstdlib>

ParseError Value::parseNumber(Context& c, Value& v)
{
    char* end;
    v.number_ = std::strtod(c.json.data(), &end);
    if (c.json.data() == end)
        return ParseError::InvalidValue;
    c.json.remove_prefix(end - c.json.data());
    v.type_ = Type::Number;
    return ParseError::OK;
}
~~~

`std::strtod()` 的第一个参数是字符串起始地址，第二个参数是输出参数，返回转换结束后的位置。我们通过比较 `c.json.data()` 和 `end` 来判断是否转换成功。如果 `end` 没有移动，说明不是合法的数字。

然而，上面的代码还没做格式校验。我们将在练习中完善它。

加入了 number 后，value 的语法变成：

~~~
value = null / false / true / number
~~~

记得在第一单元中，我们说可以用一个字符就能得知 value 是什么类型，有 11 个字符可判断 number：

* 0-9/- ➔ number

但是，由于我们在 `parseNumber()` 内部将会校验输入是否正确的值，我们可以简单地把余下的情况都交给 `parseNumber()`：

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

## 6. 总结与练习

本单元讲述了 JSON 数字类型的语法，以及 leptjson 所采用的自行校验 + `std::strtod()` 转换为 `double` 的方案。实际上一些 JSON 库会采用更复杂的方案，例如支持 64 位带符号／无符号整数，自行实现转换。以我的个人经验，解析／生成数字类型可以说是 RapidJSON 中最难实现的部分，也是 RapidJSON 高效性能的原因，有机会再另外撰文解释。

此外我们谈及，重构与单元测试是互相依赖的软件开发技术，适当地运用可提升软件的品质。之后的单元还会有相关的话题。

以下是本单元的练习：

1. 重构合并 `parseNull()`、`parseFalse()`、`parseTrue()` 为 `parseLiteral()`。
2. 加入 [维基百科双精度浮点数](https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Double-precision_examples) 的一些边界值至单元测试，如 min subnormal positive double、max double 等。
3. 按 JSON number 的语法在 `parseNumber()` 中校验，不符合标准的情况返回 `ParseError::InvalidValue` 错误码。建议使用以下两个辅助函数来简化代码：

~~~cpp
static bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }
static bool isDigit1to9(char ch) { return ch >= '1' && ch <= '9'; }
~~~

4. 仔细阅读 [`std::strtod()`](https://en.cppreference.com/w/cpp/string/byte/strtof) 的文档，看看怎样从返回值得知数值是否过大，以返回 `ParseError::NumberTooBig` 错误码。（提示：需要 `#include <cerrno>` 和 `#include <cmath>`。）

以上最重要的是第 3 条题目，就是要校验 JSON 的数字语法。

校验的思路：我们把语法再看一遍：

~~~
number = [ "-" ] int [ frac ] [ exp ]
int = "0" / digit1-9 *digit
frac = "." 1*digit
exp = ("e" / "E") ["-" / "+"] 1*digit
~~~

逐部分处理：
- 负号：有的话跳过
- 整数：要么单个 `0`，要么 1-9 开头后跟任意个 digit
- 小数：小数点后至少一个 digit
- 指数：e/E 后可选正负号，然后至少一个 digit

校验成功后再调用 `std::strtod()` 进行转换，此时第二个参数可以传 `nullptr`。

## 参考

[1] Fowler, Martin. Refactoring: improving the design of existing code. Pearson Education India, 2009. 中译本：《重构：改善既有代码的设计》，熊节译，人民邮电出版社，2010年。

[2] Gay, David M. "Correctly rounded binary-decimal and decimal-binary conversions." Numerical Analysis Manuscript 90-10 (1990).
