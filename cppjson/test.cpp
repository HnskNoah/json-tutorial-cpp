#include "leptjson.hpp"
#include <cstdio>
#include <source_location>
#include <format>

using namespace lept;

class TestRunner
{
public:
    template <typename T, typename U>
    void expect_eq(T expect, U actual, std::source_location loc = std::source_location::current())
    {
        ++count_;
        if (expect == actual)
        {
            ++pass_;
        }
        else
        {
            if constexpr (std::is_enum_v<T>)
                std::fprintf(stderr, "%s:%u: expect: %d actual: %d\n", loc.file_name(), loc.line(),
                             static_cast<int>(expect), static_cast<int>(actual));
            else
            {
                auto msg = std::format("{}:{}: expect: {} actual: {}\n", loc.file_name(),
                                       loc.line(), expect, actual);
                std::fputs(msg.c_str(), stderr);
            }
            ret_ = 1;
        }
    }

    int count() const { return count_; }
    int pass() const { return pass_; }
    int ret() const { return ret_; }

private:
    int count_ = 0;
    int pass_ = 0;
    int ret_ = 0;
};

static TestRunner runner;

static void test_error(ParseError error, std::string_view json)
{
    Value v;
    runner.expect_eq(error, v.parse(json));
    runner.expect_eq(Type::Null, v.type());
}
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
    test_roundtrip("\"Hello\\nWorld\"");
    test_roundtrip("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"");
    test_roundtrip("\"Hello\\u0020World\"");
    // ...
}

// 定义通用的测试逻辑
auto test_parse_lambda = [](ParseError error, Type type, std::string_view json)
{
    Value v;
    runner.expect_eq(error, v.parse(json));
    runner.expect_eq(type, v.type());
};

// 使用 Lambda 定义具体测试（虽然不是 using，但非常简短）
auto test_parse_null = []() { test_parse_lambda(ParseError::OK, Type::Null, "null"); };
auto test_parse_true = []() { test_parse_lambda(ParseError::OK, Type::True, "true"); };
auto test_parse_false = []() { test_parse_lambda(ParseError::OK, Type::False, "false"); };
auto test_parse_root_not_singular = []()
{ test_parse_lambda(ParseError::RootNotSingular, Type::Null, "null x"); };
auto test_parse_expect_value = []()
{
    test_parse_lambda(ParseError::ExpectValue, Type::Null, "");
    test_parse_lambda(ParseError::ExpectValue, Type::Null, " ");
};

static void test_number(double expect, std::string_view json)
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse(json));
    runner.expect_eq(Type::Number, v.type());
    runner.expect_eq(expect, v.number());
}

static void test_parse_invalid_value()
{
    test_error(ParseError::ExpectValue, "");
    test_error(ParseError::ExpectValue, " ");
    test_error(ParseError::InvalidValue, "+0");
    test_error(ParseError::InvalidValue, "+1");
    test_error(ParseError::InvalidValue, ".123"); /* at least one digit before '.' */
    test_error(ParseError::InvalidValue, "1.");   /* at least one digit after '.' */
    test_error(ParseError::InvalidValue, "INF");
    test_error(ParseError::InvalidValue, "inf");
    test_error(ParseError::InvalidValue, "NAN");
    test_error(ParseError::InvalidValue, "nan");
}

static void test_parse_array()
{
    Value v;

    v = Value();
    runner.expect_eq(ParseError::OK, v.parse("[ ]"));
    runner.expect_eq(Type::Array, v.type());
    runner.expect_eq(0, static_cast<int>(v.arraySize()));

    v = Value();
    runner.expect_eq(ParseError::OK, v.parse("[ null , false , true , 123 , \"abc\" ]"));
    runner.expect_eq(Type::Array, v.type());
    runner.expect_eq(5, static_cast<int>(v.arraySize()));
    runner.expect_eq(Type::Null, v[0].type());
    runner.expect_eq(Type::False, v[1].type());
    runner.expect_eq(Type::True, v[2].type());
    runner.expect_eq(Type::Number, v[3].type());
    runner.expect_eq(Type::String, v[4].type());
    runner.expect_eq(123.0, v[3].number());
    runner.expect_eq("abc", v[4].string());

    v = Value();
    runner.expect_eq(ParseError::OK, v.parse("[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    runner.expect_eq(Type::Array, v.type());
    runner.expect_eq(4, static_cast<int>(v.arraySize()));
    for (size_t i = 0; i < 4; i++)
    {
        runner.expect_eq(Type::Array, v[i].type());
        runner.expect_eq(static_cast<int>(i), static_cast<int>(v[i].arraySize()));
        for (size_t j = 0; j < i; j++)
        {
            runner.expect_eq(Type::Number, v[i][j].type());
            runner.expect_eq(static_cast<double>(j), v[i][j].number());
        }
    }
}
static void test_parse_number()
{
    test_number(0.0, "0");
    test_number(0.0, "-0");
    test_number(0.0, "-0.0");
    test_number(1.0, "1");
    test_number(-1.0, "-1");
    test_number(1.5, "1.5");
    test_number(-1.5, "-1.5");
    test_number(3.1416, "3.1416");
    test_number(1E10, "1E10");
    test_number(1e10, "1e10");
    test_number(1E+10, "1E+10");
    test_number(1E-10, "1E-10");
    test_number(-1E10, "-1E10");
    test_number(-1e10, "-1e10");
    test_number(-1E+10, "-1E+10");
    test_number(-1E-10, "-1E-10");
    test_number(1.234E+10, "1.234E+10");
    test_number(1.234E-10, "1.234E-10");
    test_number(0.0, "1e-10000"); /* must underflow */

    test_number(1.0000000000000002, "1.0000000000000002");           /* the smallest number > 1 */
    test_number(4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    test_number(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    test_number(2.2250738585072009e-308, "2.2250738585072009e-308"); /* Max subnormal double */
    test_number(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    test_number(2.2250738585072014e-308,
                "2.2250738585072014e-308"); /* Min normal positive double */
    test_number(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    test_number(1.7976931348623157e+308, "1.7976931348623157e+308"); /* Max double */
    test_number(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}
auto test_string(std::string_view expected, std::string_view json) -> void
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse(json));
    runner.expect_eq(Type::String, v.type());
    runner.expect_eq(expected, v.string());
};
static void test_parse_string()
{
    test_string("", "\"\"");
    test_string("Hello", "\"Hello\"");
    test_string("Hello\nWorld", "\"Hello\\nWorld\"");
    test_string("Hello\"World", "\"Hello\\\"World\"");
    test_string("Hello\\World", "\"Hello\\\\World\"");
    test_string("Hello/World", "\"Hello\\/World\"");
    test_string("Hello\bWorld", "\"Hello\\bWorld\"");
    test_string("Hello\fWorld", "\"Hello\\fWorld\"");
    test_string("Hello\rWorld", "\"Hello\\rWorld\"");
    test_string("Hello\tWorld", "\"Hello\\tWorld\"");
    test_error(ParseError::InvalidStringEscape, "\"\\a\"");
    test_error(ParseError::InvalidStringChar, "\"\x01\"");
    test_error(ParseError::MissQuotationMark, "\"abc");
}
static void test_parse()
{
    /* 练习：取消注释以下两行 */
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_expect_value();
    test_parse_root_not_singular();
    test_parse_invalid_value();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_stringify();
}

int main()
{
    test_parse();
    std::printf("%d/%d (%3.2f%%) passed\n", runner.pass(), runner.count(),
                runner.pass() * 100.0 / runner.count());
    return runner.ret();
}
