# 从零开始的 JSON 库教程（七）：生成器

本文是《从零开始的 JSON 库教程》的第七个单元。

本单元内容：

1. [JSON 生成器](#1-json-生成器)
2. [生成 null、false 和 true](#2-生成-nullfalse-和-true)
3. [生成数字](#3-生成数字)
4. [生成字符串](#4-生成字符串)
5. [生成数组和对象](#5-生成数组和对象)
6. [总结与练习](#6-总结与练习)

## 1. JSON 生成器

我们在前 6 个单元实现了一个符合标准的 JSON 解析器，把 JSON 文本解析成树形数据结构。JSON 生成器（generator）负责相反的事情——把树形数据结构转换成 JSON 文本。这个过程又称为「字符串化（stringify）」。

相对于解析器，生成器更容易实现，而且几乎不会造成运行时错误。API 设计如下：

~~~cpp
std::string Value::stringify() const;
~~~

直接返回 JSON 字符串，无需手动管理内存。

为简单起见，我们不做换行、缩进等美化处理，生成的 JSON 是单行、无空白字符的最紧凑形式。

## 2. 生成 null、false 和 true

先写测试。我们采用往返（roundtrip）测试：解析一个 JSON，再生成 JSON，比较两者是否一致：

~~~cpp
static void test_roundtrip(std::string_view json)
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse(json));
    runner.expect_eq(json, v.stringify());
}

static void test_stringify()
{
    test_roundtrip("null");
    test_roundtrip("false");
    test_roundtrip("true");
    // ...
}
~~~

注意：同一个 JSON 的内容可以有多种表示方式（如插入空白、数字 `1.0` 和 `1`），roundtrip 测试只适用于最紧凑形式的 JSON。更严格的测试方式是比较两次解析后的树是否相同，这将在下一单元实现。

然后实现 `stringify()`：

~~~cpp
std::string Value::stringify() const
{
    std::string result;
    stringifyValue(result);
    return result;
}

void Value::stringifyValue(std::string& out) const
{
    switch (type_)
    {
    case Type::Null:   out += "null";  break;
    case Type::False:  out += "false"; break;
    case Type::True:   out += "true";  break;
    // ...
    }
}
~~~

## 3. 生成数字

使用 `std::to_string()` 或 `std::snprintf()` 把浮点数转换成文本。`"%.17g"` 格式足够把双精度浮点数还原：

~~~cpp
case Type::Number:
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.17g", number_);
    out += buffer;
    break;
}
~~~

## 4. 生成字符串

字符串生成需要处理转义字符。一些字符必须转义，码点小于 0x20 的字符要转义为 `\u00XX` 形式：

~~~cpp
static void stringifyString(std::string& out, const std::string& s)
{
    out += '"';
    for (unsigned char ch : s)
    {
        switch (ch)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (ch < 0x20)
            {
                char buffer[7];
                std::snprintf(buffer, sizeof(buffer), "\\u%04X", ch);
                out += buffer;
            }
            else
                out += static_cast<char>(ch);
        }
    }
    out += '"';
}
~~~

在 `stringifyValue()` 中：

~~~cpp
case Type::String:
    stringifyString(out, str_);
    break;
~~~

## 5. 生成数组和对象

生成数组和对象都需要递归调用 `stringifyValue()`：

~~~cpp
case Type::Array:
    out += '[';
    for (size_t i = 0; i < array_.size(); i++)
    {
        if (i > 0) out += ',';
        array_[i].stringifyValue(out);
    }
    out += ']';
    break;

case Type::Object:
    out += '{';
    for (size_t i = 0; i < object_.size(); i++)
    {
        if (i > 0) out += ',';
        stringifyString(out, object_[i].key);
        out += ':';
        object_[i].value.stringifyValue(out);
    }
    out += '}';
    break;
~~~

## 6. 总结与练习

本单元实现了 JSON 生成器。得益于 C++ 的 `std::string`，我们不需要手动管理输出缓冲区的内存，代码简洁且安全。

1. 补全 `stringifyValue()` 中所有类型的生成，确保 roundtrip 测试通过。
2. 测试一些包含 Unicode 转义的字符串，确保生成时正确转义。
3. 思考：`stringifyString()` 中每个字符都调用 `out +=`，是否可以优化？比如先计算输出长度再 `reserve`？这种优化有什么代价？
