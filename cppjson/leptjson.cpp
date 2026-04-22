#include "leptjson.hpp"
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cmath>

namespace lept
{
static bool isDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}
static bool isDigit1to9(char ch)
{
    return ch >= '1' && ch <= '9';
}

void Value::free()
{
    switch (type_)
    {
    case Type::String:
        str_.clear();
        str_.shrink_to_fit();
        break;
    case Type::Array:
        array_.clear();
        array_.shrink_to_fit();
        break;
    default:
        break;
    }
    type_ = Type::Null;
}

void Value::skipWhitespace(Context& c)
{
    while (!c.json.empty())
    {
        char ch = c.json.front();
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            c.json.remove_prefix(1);
        else
            break;
    }
}

ParseError Value::parseLiteral(Context& c, Value& v, std::string_view literal, Type type)
{
    if (c.json.size() < literal.size() || c.json.substr(0, literal.size()) != literal)
    {
        return ParseError::InvalidValue;
    }
    c.json.remove_prefix(literal.size());
    v.type_ = type;
    return ParseError::OK;
}
ParseError Value::parseNumber(Context& c, Value& v)
{
    const char* start = c.json.data();
    char* end = nullptr;
    auto p = c.json;
    if (!p.empty() && p.front() == '-')
        p.remove_prefix(1);
    if (p.empty())
        return ParseError::InvalidValue;

    if (p.front() == '0')
        p.remove_prefix(1);
    else
    {
        if (!isDigit1to9(p.front()))
            return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }

    if (!p.empty() && p.front() == '.')
    {
        p.remove_prefix(1);
        if (p.empty() || !isDigit(p.front()))
            return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }

    if (!p.empty() && (p.front() == 'e' || p.front() == 'E'))
    {
        p.remove_prefix(1);
        if (!p.empty() && (p.front() == '+' || p.front() == '-'))
            p.remove_prefix(1);
        if (p.empty() || !isDigit(p.front()))
            return ParseError::InvalidValue;
        p.remove_prefix(1);
        while (!p.empty() && isDigit(p.front()))
            p.remove_prefix(1);
    }
    errno = 0;
    v.number_ = std::strtod(start, &end);
    if (errno == ERANGE && (v.number_ == HUGE_VAL || v.number_ == -HUGE_VAL))
        return ParseError::NumberTooBig;
    if (c.json.data() == end)
        return ParseError::InvalidValue;
    c.json.remove_prefix(end - c.json.data());
    v.type_ = Type::Number;
    return ParseError::OK;
}

/* value = null / false / true */
/* 练习：下面代码没处理 false / true，将会是练习之一 */
ParseError Value::parseValue(Context& c, Value& v)
{
    char ch = c.json.empty() ? '\0' : c.json.front();
    switch (ch)
    {
    case 'n':
        return parseLiteral(c, v, "null", Type::Null);
    case 't':
        return parseLiteral(c, v, "true", Type::True);
    case 'f':
        return parseLiteral(c, v, "false", Type::False);
    case '\0':
        return ParseError::ExpectValue;
    case '"':
        return parseString(c, v);
    case '[':
        return parseArray(c, v);
    default:
        return parseNumber(c, v);
    }
}

/* 提示：这里应该是 JSON-text = ws value ws */
/* 以下实现没处理最后的 ws 和 LEPT_PARSE_ROOT_NOT_SINGULAR */
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
ParseError Value::parseString(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == '"');
    c.json.remove_prefix(1);
    size_t head = c.stack.size();
    while (!c.json.empty())
    {
        char ch = c.json.front();
        c.json.remove_prefix(1);
        switch (ch)
        {
        case '"':
            v.str_ = c.popString(c.stack.size() - head);
            v.type_ = Type::String;
            return ParseError::OK;

        case '\\':
            if (c.json.empty())
            {
                c.stack.resize(head);
                return ParseError::InvalidStringEscape;
            }
            switch (c.json.front())
            {
            case '"':
                c.pushChar('"');
                c.json.remove_prefix(1);
                break;
            case '\\':
                c.pushChar('\\');
                c.json.remove_prefix(1);
                break;
            case '/':
                c.pushChar('/');
                c.json.remove_prefix(1);
                break;
            case 'b':
                c.pushChar('\b');
                c.json.remove_prefix(1);
                break;
            case 'f':
                c.pushChar('\f');
                c.json.remove_prefix(1);
                break;
            case 'n':
                c.pushChar('\n');
                c.json.remove_prefix(1);
                break;
            case 'r':
                c.pushChar('\r');
                c.json.remove_prefix(1);
                break;
            case 't':
                c.pushChar('\t');
                c.json.remove_prefix(1);
                break;
            // case 'u': 留待第四单元处理
            case 'u':
            {
                unsigned int u;
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
                    unsigned int u2;
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
                c.encodeUtf8(u);
                break;
            }
            default:
                c.stack.resize(head);
                return ParseError::InvalidStringEscape;
            }
            break;
        case '\0':
            c.stack.resize(head);
            return ParseError::MissQuotationMark;
        default:
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                c.stack.resize(head);
                return ParseError::InvalidStringChar;
            }
            c.pushChar(ch);
            break;
        }
    }
    c.stack.resize(head);
    return ParseError::MissQuotationMark;
}

ParseError Value::parseArray(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == '[');
    c.json.remove_prefix(1); // skip '['
    skipWhitespace(c);

    if (!c.json.empty() && c.json.front() == ']')
    {
        c.json.remove_prefix(1);
        v.type_ = Type::Array;
        v.array_.clear();
        return ParseError::OK;
    }

    size_t head = c.stack.size();
    size_t size = 0;
    v.type_ = Type::Array;
    v.array_.clear();

    for (;;)
    {
        // 解析一个值到临时变量
        Value e;
        ParseError ret = parseValue(c, e);
        if (ret != ParseError::OK)
        {
            // 释放栈上的临时值
            for (size_t i = 0; i < size; i++)
            {
                auto* pv =
                    reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ret;
        }

        // 把临时值压栈
        size_t oldSize = c.stack.size();
        c.stack.resize(oldSize + sizeof(Value));
        new (c.stack.data() + oldSize) Value(std::move(e));
        size++;

        skipWhitespace(c);

        if (!c.json.empty() && c.json.front() == ',')
        {
            c.json.remove_prefix(1);
            skipWhitespace(c);
        }
        else if (!c.json.empty() && c.json.front() == ']')
        {
            c.json.remove_prefix(1);
            v.type_ = Type::Array;
            v.array_.resize(size);
            // 从栈中弹出元素
            for (size_t i = size; i > 0; i--)
            {
                auto* pv =
                    reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                v.array_[i - 1] = std::move(*pv);
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ParseError::OK;
        }
        else
        {
            // 释放栈上的临时值
            for (size_t i = 0; i < size; i++)
            {
                auto* pv =
                    reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ParseError::MissCommaOrSquareBracket;
        }
    }
}
ParseError Value::parseHex4(std::string_view& json, unsigned& u)
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
} // namespace lept