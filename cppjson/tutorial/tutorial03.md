# 从零开始的 JSON 库教程（三）：解析字符串

本文是《从零开始的 JSON 库教程》的第三个单元。

本单元内容：

1. [JSON 字符串语法](#1-json-字符串语法)
2. [字符串表示](#2-字符串表示)
3. [内存管理](#3-内存管理)
4. [缓冲区与堆栈](#4-缓冲区与堆栈)
5. [解析字符串](#5-解析字符串)
6. [总结与练习](#6-总结与练习)

## 1. JSON 字符串语法

JSON 的字符串语法和 C 语言很相似，都是以双引号把字符括起来，如 `"Hello"`。但字符串采用了双引号作分隔，那么怎样可以在字符串中插入一个双引号？把 `a"b` 写成 `"a"b"` 肯定不行，都不知道那里是字符串的结束了。因此，我们需要引入转义字符（escape character），JSON 使用 `\`（反斜线）作为转义字符，那么 `"` 在字符串中就表示为 `\"`，`a"b` 的 JSON 字符串则写成 `"a\"b"`。JSON 共支持 9 种转义序列：

~~~
string = quotation-mark *char quotation-mark
char = unescaped /
   escape (
       %x22 /          ; "    quotation mark  U+0022
       %x5C /          ; \    reverse solidus U+005C
       %x2F /          ; /    solidus         U+002F
       %x62 /          ; b    backspace       U+0008
       %x66 /          ; f    form feed       U+000C
       %x6E /          ; n    line feed       U+000A
       %x72 /          ; r    carriage return U+000D
       %x74 /          ; t    tab             U+0009
       %x75 4HEXDIG )  ; uXXXX                U+XXXX
escape = %x5C          ; \
quotation-mark = %x22  ; "
unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
~~~

JSON 字符串是由前后两个双引号夹着零至多个字符。字符分为无转义字符或转义序列。转义序列有 9 种，都是以反斜线开始，如常见的 `\n` 代表换行符。比较特殊的是 `\uXXXX`，当中 XXXX 为 16 进位的 UTF-16 编码，本单元将不处理这种转义序列，留待下回分解。

无转义字符就是普通的字符。要注意的是，该范围不包括 0 至 31、双引号和反斜线，这些码点都必须要使用转义方式表示。

## 2. 字符串表示

JSON 字符串是允许含有空字符的，例如这个 JSON `"Hello\u0000World"` 就是单个字符串，解析后为 11 个字符。如果纯粹使用空结尾字符串来表示解析后的结果，就没法处理空字符。

因此，我们需要分配内存来储存解析后的字符，以及记录字符的数目（即字符串长度）。同时，大部分程序都假设字符串是空结尾字符串，我们还是在最后加上一个空字符，那么不需处理 `\u0000` 的应用可以简单地把它当作空结尾字符串。

我们为 `Value` 类添加字符串存储。由于一个值不可能同时为数字和字符串，我们可以利用 C++ 的优势，用 `std::string` 来管理字符串的内存：

~~~cpp
class Value
{
public:
    // ...
    const std::string& string() const
    {
        assert(type_ == Type::String);
        return str_;
    }
    std::string& string()
    {
        assert(type_ == Type::String);
        return str_;
    }

private:
    Type type_;
    double number_;
    std::string str_;
};
~~~

使用 `std::string` 的好处是：它自动管理内存的分配和释放，不需要手动 `malloc`/`free`。当 `Value` 对象被销毁或重新赋值时，`std::string` 的析构函数会自动释放内存。

## 3. 内存管理

有了 `std::string`，字符串的内存管理变得非常简单。我们需要一个 `free()` 成员函数来重置 `Value`，释放其持有的资源：

~~~cpp
void Value::free()
{
    switch (type_)
    {
    case Type::String:
        str_.clear();
        str_.shrink_to_fit();
        break;
    // 后续会加入数组、对象的释放
    default:
        break;
    }
    type_ = Type::Null;
}
~~~

实际上，由于 `std::string` 的析构函数会自动释放内存，我们也可以让 `Value` 的析构函数来完成这个工作：

~~~cpp
~Value() { free(); }
~~~

这样，当 `Value` 对象离开作用域时，资源会自动释放。

对于字符串的设置，我们可以提供：

~~~cpp
void Value::setString(std::string_view s)
{
    free();
    str_ = s;
    type_ = Type::String;
}
~~~

`std::string` 可以从 `std::string_view` 构造，自动完成内存分配和复制。注意调用 `setString()` 前先 `free()` 释放旧值。

## 4. 缓冲区与堆栈

我们解析字符串（以及之后的数组、对象）时，需要把解析的结果先储存在一个临时的缓冲区，最后再用 `setString()` 把缓冲区的结果设进值之中。在完成解析一个字符串之前，这个缓冲区的大小是不能预知的。

如果每次解析字符串时，都重新建一个缓冲区，那么是比较耗时的。我们可以重用这个缓冲区，每次解析 JSON 时就只需要创建一个。而且我们将会发现，无论是解析字符串、数组或对象，我们也只需要以先进后出的方式访问这个缓冲区。换句话说，我们需要一个动态的堆栈（stack）数据结构。

C++ 的 `std::vector<char>` 天然就是一个动态数组，可以用作堆栈：

~~~cpp
struct Context
{
    std::string_view json;
    std::vector<char> stack;
    explicit Context(std::string_view j) : json(j) {}
};
~~~

然后，我们实现堆栈的压入及弹出操作：

~~~cpp
static void pushChar(Context& c, char ch)
{
    c.stack.push_back(ch);
}

static char popChar(Context& c)
{
    char ch = c.stack.back();
    c.stack.pop_back();
    return ch;
}
~~~

我们还可以提供一个弹出多个字符的函数，用于取出整个字符串：

~~~cpp
static std::string popString(Context& c, size_t len)
{
    assert(c.stack.size() >= len);
    std::string result(c.stack.end() - len, c.stack.end());
    c.stack.resize(c.stack.size() - len);
    return result;
}
~~~

在 `parse()` 中，解析完成后我们断言堆栈应为空：

~~~cpp
ParseError Value::parse(std::string_view json)
{
    Context c(json);
    type_ = Type::Null;
    skipWhitespace(c);
    ParseError ret = parseValue(c, *this);
    if (ret == ParseError::OK)
    {
        skipWhitespace(c);
        if (!c.json.empty())
            return ParseError::RootNotSingular;
    }
    assert(c.stack.empty());
    return ret;
}
~~~

## 5. 解析字符串

有了以上工具，解析字符串的任务就变得很简单。我们只需要记录栈顶位置，然后把解析到的字符压栈，最后计算出长度并一次性把所有字符弹出，再设置至值里便可以。

我们还需要为字符串解析定义新的错误码：

~~~cpp
enum class ParseError
{
    OK = 0,
    ExpectValue,
    InvalidValue,
    RootNotSingular,
    NumberTooBig,
    MissQuotationMark,      // 缺少结束引号
    InvalidStringEscape,    // 非法转义序列
    InvalidStringChar       // 非法字符（控制字符）
};
~~~

以下是部分实现，没有处理转义和一些不合法字符的校验：

~~~cpp
ParseError Value::parseString(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == '"');
    c.json.remove_prefix(1); // skip opening '"'
    size_t head = c.stack.size();
    while (!c.json.empty())
    {
        char ch = c.json.front();
        c.json.remove_prefix(1);
        switch (ch)
        {
        case '"':
            v.str_ = popString(c, c.stack.size() - head);
            v.type_ = Type::String;
            return ParseError::OK;
        case '\0':
            c.stack.resize(head);
            return ParseError::MissQuotationMark;
        default:
            pushChar(c, ch);
        }
    }
    c.stack.resize(head);
    return ParseError::MissQuotationMark;
}
~~~

注意出错时需要把堆栈恢复到 `head` 位置，避免影响后续解析。

## 6. 总结与练习

之前的单元都是固定长度的数据类型，而字符串类型是可变长度的数据类型，因此本单元花了较多篇幅讲述内存管理和数据结构的设计。利用 C++ 的 `std::string` 和 `std::vector`，我们避免了手动 `malloc`/`free` 的繁琐和容易出错的问题。

以下是本单元的练习：

1. 实现 `parseString()` 中除 `\u` 以外的转义序列解析。遇到不合法的转义序列返回 `ParseError::InvalidStringEscape`。
2. 检查不合法字符（码点 0x00 ~ 0x1F），返回 `ParseError::InvalidStringChar`。
3. 编写 `test_parse_string()` 单元测试，包含各种合法和不合法的字符串。
4. 思考如何优化 `parseString()` 的性能，那些优化方法有没有缺点。
