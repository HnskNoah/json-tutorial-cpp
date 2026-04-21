# 从零开始的 JSON 库教程（三）：解析字符串解答篇

本文是第三个单元的解答篇。

## 1. 转义序列的解析

转义序列的解析很直观，遇到 `\` 后根据下一个字符进行处理，对不合法的转义返回 `ParseError::InvalidStringEscape`：

~~~cpp
case '\\':
    if (c.json.empty())
    {
        c.stack.resize(head);
        return ParseError::InvalidStringEscape;
    }
    switch (c.json.front())
    {
    case '"':  pushChar(c, '"');  c.json.remove_prefix(1); break;
    case '\\': pushChar(c, '\\'); c.json.remove_prefix(1); break;
    case '/':  pushChar(c, '/');  c.json.remove_prefix(1); break;
    case 'b':  pushChar(c, '\b'); c.json.remove_prefix(1); break;
    case 'f':  pushChar(c, '\f'); c.json.remove_prefix(1); break;
    case 'n':  pushChar(c, '\n'); c.json.remove_prefix(1); break;
    case 'r':  pushChar(c, '\r'); c.json.remove_prefix(1); break;
    case 't':  pushChar(c, '\t'); c.json.remove_prefix(1); break;
    // case 'u': 留待第四单元处理
    default:
        c.stack.resize(head);
        return ParseError::InvalidStringEscape;
    }
    break;
~~~

## 2. 不合法字符检查

从语法可知，不合法的字符是码点 0x00 ~ 0x1F（控制字符）。在 `default` 分支中处理：

~~~cpp
default:
    if (static_cast<unsigned char>(ch) < 0x20)
    {
        c.stack.resize(head);
        return ParseError::InvalidStringChar;
    }
    pushChar(c, ch);
    break;
~~~

注意 `char` 是否带符号是实现定义的，如果带符号则大于 0x7F 的字符会变成负数。转换为 `unsigned char` 可以避免这个问题。

## 3. 单元测试

字符串的测试用例应包含：

~~~cpp
static void test_parse_string()
{
    test_string("", "");
    test_string("Hello", "Hello");
    test_string("Hello\\nWorld", "Hello\nWorld");
    test_string("Hello\\\"World", "Hello\"World");
    test_string("Hello\\\\World", "Hello\\World");
    test_string("Hello\\/World", "Hello/World");
    test_string("Hello\\bWorld", "Hello\bWorld");
    test_string("Hello\\fWorld", "Hello\fWorld");
    test_string("Hello\\rWorld", "Hello\rWorld");
    test_string("Hello\\tWorld", "Hello\tWorld");
    test_error(ParseError::InvalidStringEscape, "\"\\a\"");
    test_error(ParseError::InvalidStringChar, "\"\x01\"");
    test_error(ParseError::MissQuotationMark, "\"abc");
}
~~~

## 4. 性能优化的思考

这是开放式问题，没有标准答案。一些思路：

1. 如果整个字符串都没有转义符，字符被复制了两次（从输入到堆栈，从堆栈到 `std::string`）。可以先扫描是否有转义字符，无转义的部分可以直接复制。缺点是代码更复杂。
2. 对于扫描无转义部分，可以考虑用 SIMD 加速。缺点是不跨平台。
3. `std::vector` 的 `push_back` 每次都检查容量是否足够。可以预先 `reserve` 估计的大小来减少检查次数。缺点是可能浪费内存。
