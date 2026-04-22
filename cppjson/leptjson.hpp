#pragma once

#include <cassert>
#include <cstddef>
#include <map>
#include <string_view>
#include <string>
#include <vector>

namespace lept
{

enum class Type
{
    Null,
    False,
    True,
    Number,
    String,
    Array,
    Object
};

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

class Value
{
public:
    Value() : type_(Type::Null), number_(0.0), str_() {}

    ParseError parse(std::string_view json);
    Type type() const { return type_; }

    double number() const
    {
        assert(type_ == Type::Number);
        return number_;
    }

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

    void setString(std::string_view s)
    {
        free();
        str_ = s;
        type_ = Type::String;
    }

    void free();

    bool is_null() const { return type_ == Type::Null; }
    bool is_false() const { return type_ == Type::False; }
    bool is_true() const { return type_ == Type::True; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    size_t arraySize() const
    {
        assert(type_ == Type::Array);
        return array_.size();
    }

    const Value& operator[](size_t index) const
    {
        assert(type_ == Type::Array && index < array_.size());
        return array_[index];
    }

    Value& operator[](size_t index)
    {
        assert(type_ == Type::Array && index < array_.size());
        return array_[index];
    }

    size_t objectSize() const
    {
        assert(type_ == Type::Object);
        return object_.size();
    }

    const Value& operator[](const std::string& key) const
    {
        assert(type_ == Type::Object && object_.count(key));
        return object_.at(key);
    }

    Value& operator[](const std::string& key)
    {
        assert(type_ == Type::Object && object_.count(key));
        return object_[key];
    }

private:
    Type type_;
    double number_;
    std::string str_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;

    struct Context
    {
        std::string_view json;
        std::vector<char> stack; // 需要包含 <vector>

        explicit Context(std::string_view j) : json(j) {}

        // 成员函数：直接操作 this->stack
        void pushChar(char ch) { stack.push_back(ch); }

        char popChar()
        {
            char ch = stack.back();
            stack.pop_back();
            return ch;
        }

        // 也可以把 popString 放进来
        std::string popString(size_t len)
        {
            assert(stack.size() >= len);
            std::string result(stack.end() - len, stack.end());
            stack.resize(stack.size() - len);
            return result;
        }

        void encodeUtf8(unsigned codepoint)
        {
            if (codepoint <= 0x7F)
                pushChar(static_cast<char>(codepoint));
            else if (codepoint <= 0x7FF)
            {
                pushChar(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                pushChar(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0xFFFF)
            {
                pushChar(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                pushChar(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                pushChar(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else
            {
                pushChar(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
                pushChar(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                pushChar(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                pushChar(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }
    };

    static void skipWhitespace(Context& c);

    static ParseError parseLiteral(Context& c, Value& v, std::string_view literal, Type type);
    static ParseError parseValue(Context& c, Value& v);
    static ParseError parseNumber(Context& c, Value& v);
    static ParseError parseString(Context& c, Value& v);
    static ParseError parseHex4(std::string_view& json, unsigned& u);
    static ParseError parseArray(Context& c, Value& v);
    static ParseError parseObject(Context& c, Value& v);
    static ParseError parseStringRaw(Context& c, std::string& s);
};

} // namespace lept
