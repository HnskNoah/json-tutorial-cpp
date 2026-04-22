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

static void stringifyString(std::string& out, const std::string& s)
{
    out += '"';
    for (unsigned char ch : s)
    {
        switch (ch)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;

        case '/':
            out += "\\/";
            break;

        default:
            if (ch <= 0x20)
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

void Value::stringifyValue(std::string& out) const
{
    switch (type_)
    {
    case Type::Null:
        out += "null";
        break;
    case Type::False:
        out += "false";
        break;
    case Type::True:
        out += "true";
        break;
    case Type::Number:
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.17g", number_);
        out += buffer;
        break;
    }
    case Type::String:
        stringifyString(out, str_);
        break;

    case Type::Array:
        out += '[';
        for (size_t i = 0; i < array_.size(); i++)
        {
            if (i > 0)
                out += ',';
            array_[i].stringifyValue(out);
        }
        out += ']';
        break;
    case Type::Object:
        out += '{';
        bool first = true;
        for (auto& [k, v] : object_)
        {
            if (!first)
                out += ',';
            first = false;
            stringifyString(out, k);
            out += ':';
            v.stringifyValue(out);
        }
        out += '}';
        break;
    }
}

std::string Value::stringify() const
{
    std::string out;
    stringifyValue(out);
    return out;
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
    case '{':
        return parseObject(c, v);
    default:
        return parseNumber(c, v);
    }
}

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
ParseError Value::parseStringRaw(Context& c, std::string& out)
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
            out = c.popString(c.stack.size() - head);
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
                c.json.remove_prefix(1); // 跳过 'u'
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

ParseError Value::parseString(Context& c, Value& v)
{
    std::string s;
    ParseError ret = parseStringRaw(c, s);
    if (ret != ParseError::OK)
        return ret;
    v.str_ = std::move(s);
    v.type_ = Type::String;
    return ParseError::OK;
}

ParseError Value::parseObject(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == '{');
    c.json.remove_prefix(1);
    skipWhitespace(c);

    if (!c.json.empty() && c.json.front() == '}')
    {
        c.json.remove_prefix(1);
        v.type_ = Type::Object;
        v.object_.clear();
        return ParseError::OK;
    }

    v.type_ = Type::Object;
    v.object_.clear();

    for (;;)
    {
        // 1. parse key
        if (c.json.empty() || c.json.front() != '"')
            return ParseError::MissKey;

        std::string key;
        ParseError ret = parseStringRaw(c, key);
        if (ret != ParseError::OK)
            return ret;

        // 2. parse ws colon ws
        skipWhitespace(c);
        if (c.json.empty() || c.json.front() != ':')
            return ParseError::MissColon;
        c.json.remove_prefix(1);
        skipWhitespace(c);

        // 3. parse value
        Value val;
        ret = parseValue(c, val);
        if (ret != ParseError::OK)
            return ret;

        v.object_[std::move(key)] = std::move(val);

        skipWhitespace(c);

        // 4. comma or }
        if (!c.json.empty() && c.json.front() == ',')
        {
            c.json.remove_prefix(1);
            skipWhitespace(c);
        }
        else if (!c.json.empty() && c.json.front() == '}')
        {
            c.json.remove_prefix(1);
            return ParseError::OK;
        }
        else
            return ParseError::MissCommaOrCurlyBracket;
    }
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

            return ret;
        }

        v.array_.push_back(std::move(e));
        skipWhitespace(c);

        if (!c.json.empty() && c.json.front() == ',')
        {
            c.json.remove_prefix(1);
            skipWhitespace(c);
        }
        else if (!c.json.empty() && c.json.front() == ']')
        {
            c.json.remove_prefix(1);
            return ParseError::OK;
        }
        else
        {
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