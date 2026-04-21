#pragma once

#include <cstddef>
#include <string_view>

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
    RootNotSingular
};

class Value
{
public:
    Value() : type_(Type::Null) {}

    ParseError parse(std::string_view json);
    Type type() const { return type_; }

    bool is_null() const { return type_ == Type::Null; }
    bool is_false() const { return type_ == Type::False; }
    bool is_true() const { return type_ == Type::True; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

private:
    Type type_;

    struct Context
    {
        std::string_view json;
        explicit Context(std::string_view j) : json(j) {}
    };

    static void skipWhitespace(Context& c);
    static ParseError parseNull(Context& c, Value& v);
    /* 练习：取消注释并实现以下函数声明 */
    static ParseError parseTrue(Context& c, Value& v);
    static ParseError parseFalse(Context& c, Value& v);
    static ParseError parseValue(Context& c, Value& v);
};

} // namespace lept
