#pragma once

#include <cassert>
#include <cstddef>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace lept
{

enum class ParseError
{
    OK = 0,
    ExpectValue,
    InvalidValue,
    RootNotSingular,
    NumberTooBig,
    MissQuotationMark,
    InvalidStringEscape,
    InvalidStringChar,
    InvalidUnicodeHex,
    InvalidUnicodeSurrogate,
    MissCommaOrSquareBracket,
    MissKey,
    MissColon,
    MissCommaOrCurlyBracket
};

// overloaded 辅助工具
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class Value
{
public:
    using Array = std::vector<Value>;
    using Object = std::unordered_map<std::string, Value>;
    using Data = std::variant<std::monostate, bool, double, std::string, Array, Object>;

    Value() = default;

    [[nodiscard]] static std::expected<Value, ParseError> parse(std::string_view json)
    {
        Value v;
        auto err = v.parseImpl(json);
        if (err != ParseError::OK)
            return std::unexpected(err);
        return v;
    }

    // 类型查询
    bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_number() const { return std::holds_alternative<double>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_array() const { return std::holds_alternative<Array>(data_); }
    bool is_object() const { return std::holds_alternative<Object>(data_); }
    bool is_true() const
    {
        auto* p = std::get_if<bool>(&data_);
        return p && *p;
    }
    bool is_false() const
    {
        auto* p = std::get_if<bool>(&data_);
        return p && !*p;
    }

    // 访问器
    const double* number() const { return std::get_if<double>(&data_); }
    const std::string* string() const { return std::get_if<std::string>(&data_); }
    const Array* array() const { return std::get_if<Array>(&data_); }
    const Object* object() const { return std::get_if<Object>(&data_); }

    size_t arraySize() const
    {
        auto* a = std::get_if<Array>(&data_);
        return a ? a->size() : 0;
    }

    size_t objectSize() const
    {
        auto* o = std::get_if<Object>(&data_);
        return o ? o->size() : 0;
    }

    const Value* operator[](size_t index) const
    {
        auto* a = std::get_if<Array>(&data_);
        if (!a || index >= a->size())
            return nullptr;
        return &(*a)[index];
    }

    const Value* operator[](const std::string& key) const
    {
        auto* o = std::get_if<Object>(&data_);
        if (!o)
            return nullptr;
        auto it = o->find(key);
        if (it == o->end())
            return nullptr;
        return &it->second;
    }

    [[nodiscard]] std::string stringify() const
    {
        std::string out;
        stringifyValue(out);
        return out;
    }

    // 直接访问底层数据
    const Data& data() const { return data_; }

private:
    Data data_;

    struct Context
    {
        std::string_view json;
        std::vector<char> stack;

        explicit Context(std::string_view j) : json(j) {}

        void pushChar(char ch) { stack.push_back(ch); }

        std::string popString(size_t len)
        {
            assert(stack.size() >= len);
            std::string result(stack.end() - len, stack.end());
            stack.resize(stack.size() - len);
            return result;
        }

        void encodeUtf8(unsigned cp)
        {
            if (cp <= 0x7F)
                pushChar(static_cast<char>(cp));
            else if (cp <= 0x7FF)
            {
                pushChar(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                pushChar(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp <= 0xFFFF)
            {
                pushChar(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                pushChar(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                pushChar(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                pushChar(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
                pushChar(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                pushChar(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                pushChar(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
    };

    // RAII stack guard
    struct StackGuard
    {
        Context& c;
        size_t head;
        ~StackGuard() { c.stack.resize(head); }
    };

    // ---- parse 内部实现 ----

    [[nodiscard]] ParseError parseImpl(std::string_view json)
    {
        Context c(json);
        data_ = std::monostate{};
        skipWhitespace(c);
        auto ret = parseValue(c, *this);
        if (ret == ParseError::OK)
        {
            skipWhitespace(c);
            if (!c.json.empty())
                return ParseError::RootNotSingular;
        }
        assert(c.stack.empty());
        return ret;
    }

    static void skipWhitespace(Context& c)
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

    static auto isDigit(char ch) { return ch >= '0' && ch <= '9'; }
    static auto isDigit1to9(char ch) { return ch >= '1' && ch <= '9'; }

    static ParseError parseLiteral(Context& c, Value& v, std::string_view literal, bool val)
    {
        if (c.json.size() < literal.size() || c.json.substr(0, literal.size()) != literal)
            return ParseError::InvalidValue;
        c.json.remove_prefix(literal.size());
        v.data_ = val;
        return ParseError::OK;
    }

    static ParseError parseNumber(Context& c, Value& v)
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
        double num = std::strtod(start, &end);
        if (errno == ERANGE && (num == HUGE_VAL || num == -HUGE_VAL))
            return ParseError::NumberTooBig;
        if (c.json.data() == end)
            return ParseError::InvalidValue;
        c.json.remove_prefix(end - c.json.data());
        v.data_ = num;
        return ParseError::OK;
    }

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

    static ParseError parseStringRaw(Context& c, std::string& out)
    {
        assert(!c.json.empty() && c.json.front() == '"');
        c.json.remove_prefix(1);
        size_t head = c.stack.size();
        StackGuard guard{c, head};

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
                    return ParseError::InvalidStringEscape;
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
                case 'u':
                {
                    c.json.remove_prefix(1);
                    unsigned u;
                    if (parseHex4(c.json, u) != ParseError::OK)
                        return ParseError::InvalidUnicodeHex;
                    if (u >= 0xD800 && u <= 0xDBFF)
                    {
                        if (c.json.size() < 2 || c.json.front() != '\\')
                            return ParseError::InvalidUnicodeSurrogate;
                        c.json.remove_prefix(1);
                        if (c.json.empty() || c.json.front() != 'u')
                            return ParseError::InvalidUnicodeSurrogate;
                        c.json.remove_prefix(1);
                        unsigned u2;
                        if (parseHex4(c.json, u2) != ParseError::OK)
                            return ParseError::InvalidUnicodeHex;
                        if (u2 < 0xDC00 || u2 > 0xDFFF)
                            return ParseError::InvalidUnicodeSurrogate;
                        u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                    }
                    c.encodeUtf8(u);
                    break;
                }
                default:
                    return ParseError::InvalidStringEscape;
                }
                break;
            case '\0':
                return ParseError::MissQuotationMark;
            default:
                if (static_cast<unsigned char>(ch) < 0x20)
                    return ParseError::InvalidStringChar;
                c.pushChar(ch);
                break;
            }
        }
        return ParseError::MissQuotationMark;
    }

    static ParseError parseString(Context& c, Value& v)
    {
        std::string s;
        auto ret = parseStringRaw(c, s);
        if (ret != ParseError::OK)
            return ret;
        v.data_ = std::move(s);
        return ParseError::OK;
    }

    static ParseError parseArray(Context& c, Value& v)
    {
        assert(!c.json.empty() && c.json.front() == '[');
        c.json.remove_prefix(1);
        skipWhitespace(c);

        v.data_ = Array{};
        auto& arr = std::get<Array>(v.data_);

        if (!c.json.empty() && c.json.front() == ']')
        {
            c.json.remove_prefix(1);
            return ParseError::OK;
        }

        for (;;)
        {
            Value e;
            auto ret = parseValue(c, e);
            if (ret != ParseError::OK)
                return ret;

            arr.push_back(std::move(e));
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
                return ParseError::MissCommaOrSquareBracket;
        }
    }

    static ParseError parseObject(Context& c, Value& v)
    {
        assert(!c.json.empty() && c.json.front() == '{');
        c.json.remove_prefix(1);
        skipWhitespace(c);

        v.data_ = Object{};
        auto& obj = std::get<Object>(v.data_);

        if (!c.json.empty() && c.json.front() == '}')
        {
            c.json.remove_prefix(1);
            return ParseError::OK;
        }

        for (;;)
        {
            if (c.json.empty() || c.json.front() != '"')
                return ParseError::MissKey;

            std::string key;
            auto ret = parseStringRaw(c, key);
            if (ret != ParseError::OK)
                return ret;

            skipWhitespace(c);
            if (c.json.empty() || c.json.front() != ':')
                return ParseError::MissColon;
            c.json.remove_prefix(1);
            skipWhitespace(c);

            Value val;
            ret = parseValue(c, val);
            if (ret != ParseError::OK)
                return ret;

            obj[std::move(key)] = std::move(val);
            skipWhitespace(c);

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

    static ParseError parseValue(Context& c, Value& v)
    {
        char ch = c.json.empty() ? '\0' : c.json.front();
        switch (ch)
        {
        case 'n':
            if (c.json.size() >= 4 && c.json.substr(0, 4) == "null")
            {
                c.json.remove_prefix(4);
                v.data_ = std::monostate{};
                return ParseError::OK;
            }
            return ParseError::InvalidValue;
        case 't':
            return parseLiteral(c, v, "true", true);
        case 'f':
            return parseLiteral(c, v, "false", false);
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

    // ---- stringify 实现 ----

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
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04X", ch);
                    out += buf;
                }
                else
                    out += static_cast<char>(ch);
            }
        }
        out += '"';
    }

    void stringifyValue(std::string& out) const
    {
        std::visit(overloaded{[&](std::monostate) { out += "null"; },
                              [&](bool b) { out += b ? "true" : "false"; },
                              [&](double d)
                              {
                                  char buf[32];
                                  std::snprintf(buf, sizeof(buf), "%.17g", d);
                                  out += buf;
                              },
                              [&](const std::string& s) { stringifyString(out, s); },
                              [&](const Array& a)
                              {
                                  out += '[';
                                  for (size_t i = 0; i < a.size(); i++)
                                  {
                                      if (i > 0)
                                          out += ',';
                                      a[i].stringifyValue(out);
                                  }
                                  out += ']';
                              },
                              [&](const Object& o)
                              {
                                  out += '{';
                                  bool first = true;
                                  for (auto& [k, v] : o)
                                  {
                                      if (!first)
                                          out += ',';
                                      first = false;
                                      stringifyString(out, k);
                                      out += ':';
                                      v.stringifyValue(out);
                                  }
                                  out += '}';
                              }},
                   data_);
    }
};

} // namespace lept
