# 从零开始的 JSON 库教程（四）：Unicode 解答篇

本文是第四个单元的解答篇。

## 1. 实现 parseHex4()

解析 4 位 16 进制数字，成功时通过引用返回码点并推进 `string_view`，失败返回错误码：

~~~cpp
static ParseError parseHex4(std::string_view& json, unsigned& u)
{
    u = 0;
    for (int i = 0; i < 4; i++)
    {
        if (json.empty())
            return ParseError::InvalidUnicodeHex;
        char ch = json.front();
        json.remove_prefix(1);
        u <<= 4;
        if (ch >= '0' && ch <= '9')
            u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')
            u |= ch - ('a' - 10);
        else
            return ParseError::InvalidUnicodeHex;
    }
    return ParseError::OK;
}
~~~

为什么不使用 `std::strtol()`？因为 `strtol()` 会跳过开始的空白，会错误地接受 `\u 123` 这种不合法的 JSON。

## 2. 实现 encodeUtf8()

根据 UTF-8 编码表实现：

~~~cpp
static void encodeUtf8(Context& c, unsigned u)
{
    if (u <= 0x7F)
        pushChar(c, u & 0xFF);
    else if (u <= 0x7FF)
    {
        pushChar(c, 0xC0 | ((u >> 6) & 0xFF));
        pushChar(c, 0x80 | (u & 0x3F));
    }
    else if (u <= 0xFFFF)
    {
        pushChar(c, 0xE0 | ((u >> 12) & 0xFF));
        pushChar(c, 0x80 | ((u >> 6) & 0x3F));
        pushChar(c, 0x80 | (u & 0x3F));
    }
    else
    {
        assert(u <= 0x10FFFF);
        pushChar(c, 0xF0 | ((u >> 18) & 0xFF));
        pushChar(c, 0x80 | ((u >> 12) & 0x3F));
        pushChar(c, 0x80 | ((u >> 6) & 0x3F));
        pushChar(c, 0x80 | (u & 0x3F));
    }
}
~~~

## 3. 代理对的处理

遇到高代理项（U+D800 ~ U+DBFF），需要解析下一个 `\uXXXX` 作为低代理项，然后计算码点：

~~~cpp
case 'u':
{
    unsigned u;
    if (parseHex4(c.json, u) != ParseError::OK)
    {
        c.stack.resize(head);
        return ParseError::InvalidUnicodeHex;
    }
    if (u >= 0xD800 && u <= 0xDBFF)
    {
        // surrogate pair
        if (c.json.size() < 2 || c.json.front() != '\\')
        {
            c.stack.resize(head);
            return ParseError::InvalidUnicodeSurrogate;
        }
        c.json.remove_prefix(1); // skip '\'
        if (c.json.empty() || c.json.front() != 'u')
        {
            c.stack.resize(head);
            return ParseError::InvalidUnicodeSurrogate;
        }
        c.json.remove_prefix(1); // skip 'u'
        unsigned u2;
        if (parseHex4(c.json, u2) != ParseError::OK)
        {
            c.stack.resize(head);
            return ParseError::InvalidUnicodeHex;
        }
        if (u2 < 0xDC00 || u2 > 0xDFFF)
        {
            c.stack.resize(head);
            return ParseError::InvalidUnicodeSurrogate;
        }
        u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
    }
    encodeUtf8(c, u);
    break;
}
~~~

注意也要检查孤立的高代理项（后面没有跟 `\u`）和低代理项不在 U+DC00 ~ U+DFFF 范围的情况。
