#include "leptjson.hpp"
#include <cstdio>
#include <source_location>

using namespace lept;

class TestRunner
{
public:
    template <typename T, typename U>
    void expect_eq(T expect, U actual, std::source_location loc = std::source_location::current())
    {
        int e = static_cast<int>(expect);
        int a = static_cast<int>(actual);
        ++count_;
        if (e == a)
        {
            ++pass_;
        }
        else
        {
            std::fprintf(stderr, "%s:%u: expect: %d actual: %d\n", loc.file_name(), loc.line(), e,
                         a);
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

static void test_parse_null()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("null"));
    runner.expect_eq(Type::Null, v.type());
}

/* 练习：参考 test_parse_null()，加入 test_parse_true() */
static void test_parse_true()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("true"));
    runner.expect_eq(Type::True, v.type());
}

/* 练习：参考 test_parse_null()，加入 test_parse_false() */
static void test_parse_false()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("false"));
    runner.expect_eq(Type::False, v.type());
}

static void test_parse_expect_value()
{
    Value v;
    runner.expect_eq(ParseError::ExpectValue, v.parse(""));
    runner.expect_eq(Type::Null, v.type());

    Value v2;
    runner.expect_eq(ParseError::ExpectValue, v2.parse(" "));
    runner.expect_eq(Type::Null, v2.type());
}

static void test_parse_invalid_value()
{
    Value v;
    runner.expect_eq(ParseError::InvalidValue, v.parse("nul"));
    runner.expect_eq(Type::Null, v.type());

    Value v2;
    runner.expect_eq(ParseError::InvalidValue, v2.parse("?"));
    runner.expect_eq(Type::Null, v2.type());
}

static void test_parse_root_not_singular()
{
    Value v;
    runner.expect_eq(ParseError::RootNotSingular, v.parse("null x"));
    runner.expect_eq(Type::Null, v.type());
}

static void test_parse()
{
    test_parse_null();
    /* 练习：取消注释以下两行 */
    test_parse_true();
    test_parse_false();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
}

int main()
{
    test_parse();
    std::printf("%d/%d (%3.2f%%) passed\n", runner.pass(), runner.count(),
                runner.pass() * 100.0 / runner.count());
    return runner.ret();
}
