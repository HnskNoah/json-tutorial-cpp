# 从零开始的 JSON 库教程（四）：Unicode

本文是《从零开始的 JSON 库教程》的第四个单元。

本单元内容：

1. [Unicode 简介](#1-unicode-简介)
2. [需求](#2-需求)
3. [UTF-8 编码](#3-utf-8-编码)
4. [实现 `\uXXXX` 解析](#4-实现-uxxxx-解析)
5. [总结与练习](#5-总结与练习)

## 1. Unicode 简介

在上一个单元，我们已经能解析一般的 JSON 字符串，仅仅没有处理 `\uXXXX` 这种转义序列。为了解析这种序列，我们必须了解有关 Unicode 的基本概念。

读者应该知道 ASCII，它是一种字符编码，把 128 个字符映射至整数 0 ~ 127。例如 `1` → 49，`A` → 65。这种 7-bit 编码系统非常简单，但仅适合美国英语。

在 Unicode 出现之前，各地区制定了不同的编码系统，如中文的 GB 2312、日文的 JIS 等，造成很多不便。Unicode 联盟制定了统一字符集（UCS），每个字符映射至一个整数码点（code point），码点范围是 0 至 0x10FFFF，通常记作 U+XXXX。例如 `劲` → U+52B2、`峰` → U+5CF0。

Unicode 还制定了各种储存码点的方式，称为 Unicode 转换格式（UTF）。现时流行的 UTF 为 UTF-8、UTF-16 和 UTF-32。每种 UTF 把一个码点储存为一至多个编码单元（code unit）。除 UTF-32 外，UTF-8 和 UTF-16 都是可变长度编码。

UTF-8 成为互联网上最流行的格式，因为：

1. 以字节为编码单元，没有字节序问题。
2. ASCII 字符只需一个字节，与 ASCII 兼容。
3. 原来以字节方式储存字符的程序理论上不需要特别改动。

## 2. 需求

由于 UTF-8 的普及性，大部分 JSON 也以 UTF-8 存储。我们的 JSON 库也只支持 UTF-8。

对于非转义字符，只要它们不少于 32（0 ~ 31 是不合法的），我们可以直接复制至结果。我们假设输入是合法的 UTF-8 编码。

而对于 `\uXXXX`，它以 16 进制表示码点 U+0000 至 U+FFFF，我们需要：

1. 解析 4 位十六进制整数为码点。
2. 把码点编码成 UTF-8。

4 位的 16 进制数字只能表示 0 至 0xFFFF，但 Unicode 码点到 0x10FFFF。U+0000 至 U+FFFF 这组字符称为基本多文种平面（BMP），BMP 以外的字符，JSON 使用代理对（surrogate pair）表示 `\uXXXX\uYYYY`。

在 BMP 中，保留了 2048 个代理码点。如果第一个码点是 U+D800 至 U+DBFF，它是高代理项（high surrogate），之后应该伴随一个 U+DC00 至 U+DFFF 的低代理项（low surrogate）。然后用公式计算真实码点：

~~~c
codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00)
~~~

例如，高音谱号 `𝄞` → U+1D11E 不是 BMP 内的字符。在 JSON 中写成 `\uD834\uDD1E`：

~~~c
H = 0xD834, L = 0xDD1E
codepoint = 0x10000 + (0xD834 - 0xD800) × 0x400 + (0xDD1E - 0xDC00)
          = 0x10000 + 0x34 × 0x400 + 0x11E
          = 0x1D11E
~~~

如果只有高代理项而缺少低代理项，或者低代理项不在合法范围，我们返回 `ParseError::InvalidUnicodeSurrogate`。如果 `\u` 后不是 4 位十六进制数字，则返回 `ParseError::InvalidUnicodeHex`。

新增错误码：

~~~cpp
enum class ParseError
{
    // ...
    InvalidUnicodeHex,        // \u 后不是 4 位十六进制
    InvalidUnicodeSurrogate   // 非法代理对
};
~~~

## 3. UTF-8 编码

UTF-8 的编码单元为 8 位（1 字节），每个码点编码成 1 至 4 个字节：

| 码点范围            | 码点位数  | 字节1     | 字节2    | 字节3    | 字节4     |
|:------------------:|:--------:|:--------:|:--------:|:--------:|:--------:|
| U+0000 ~ U+007F    | 7        | 0xxxxxxx |          |          |          |
| U+0080 ~ U+07FF    | 11       | 110xxxxx | 10xxxxxx |          |          |
| U+0800 ~ U+FFFF    | 16       | 1110xxxx | 10xxxxxx | 10xxxxxx |          |
| U+10000 ~ U+10FFFF | 21       | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |

码点范围 U+0000 ~ U+007F 编码为一个字节，与 ASCII 兼容。

举例，欧元符号 `€` → U+20AC：

1. U+20AC 在 U+0800 ~ U+FFFF 的范围，编码成 3 个字节
2. U+20AC 的二进位为 `10000010101100`
3. 补零至 16 位：`0010000010101100`
4. 分 3 组：`0010`, `000010`, `101100`
5. 加前缀：`11100010`, `10000010`, `10101100`
6. 十六进位：0xE2, 0x82, 0xAC

编码函数实现：

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

`& 0xFF` 和 `& 0x3F` 操作是为了避免编译器对 `unsigned` 到 `char` 截断的警告，优化后不会有性能影响。

## 4. 实现 `\uXXXX` 解析

我们只需要在转义符的处理中加入对 `\u` 的处理。首先实现解析 4 位十六进制数的函数：

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

注意我们通过引用修改 `json`（即 `Context` 的 `string_view`），解析成功时自动前进。解析失败时返回错误码。

然后在 `parseString()` 的转义处理中加入：

~~~cpp
case 'u':
{
    unsigned int u;
    if (parseHex4(c.json, u) != ParseError::OK)
    {
        c.stack.resize(head);
        return ParseError::InvalidUnicodeHex;
    }
    // 代理对处理留作练习
    encodeUtf8(c, u);
    break;
}
~~~

## 5. 总结与练习

本单元介绍了 Unicode 的基本知识，同学应该了解到码点、编码单元、UTF-8、代理对等概念。

1. 实现 `parseHex4()`，不合法的十六进制数返回 `ParseError::InvalidUnicodeHex`。
2. 按第 3 节的 UTF-8 编码原理，实现 `encodeUtf8()`。
3. 加入对代理对的处理，不正确的代理对范围返回 `ParseError::InvalidUnicodeSurrogate`。
