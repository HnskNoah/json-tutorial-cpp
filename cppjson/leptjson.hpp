#pragma once

#include <cassert>
#include <cstddef>
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

private:
    Type type_;
    double number_;
    std::string str_;

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
    };

    static void skipWhitespace(Context& c);

    static ParseError parseLiteral(Context& c, Value& v, std::string_view literal, Type type);
    static ParseError parseValue(Context& c, Value& v);
    static ParseError parseNumber(Context& c, Value& v);
    static ParseError parseString(Context& c, Value& v);
};

} // namespace lept
