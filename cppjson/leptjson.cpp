#include "leptjson.hpp"
#include <cassert>

namespace lept
{

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

/* null = "null" */
ParseError Value::parseNull(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == 'n');
    c.json.remove_prefix(1); // skip 'n'
    if (c.json.size() < 3 || c.json.substr(0, 3) != "ull")
        return ParseError::InvalidValue;
    c.json.remove_prefix(3);
    v.type_ = Type::Null;
    return ParseError::OK;
}

/* 练习：参考 parseNull() 实现 parseTrue() */
/* true = "true" */
ParseError Value::parseTrue(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == 't');
    c.json.remove_prefix(1); // skip 't'
    if (c.json.size() < 3 || c.json.substr(0, 3) != "rue")
        return ParseError::InvalidValue;
    c.json.remove_prefix(3);
    v.type_ = Type::True;
    return ParseError::OK;
}
/* 练习：参考 parseNull() 实现 parseFalse() */
/* false = "false" */
ParseError Value::parseFalse(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == 'f');
    c.json.remove_prefix(1); // skip 'f'
    if (c.json.size() < 4 || c.json.substr(0, 4) != "alse")
        return ParseError::InvalidValue;
    c.json.remove_prefix(4);
    v.type_ = Type::False;
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
        return parseNull(c, v);
    /* 练习：加入 't' 和 'f' 的分支 */
    case 't':
        return parseTrue(c, v);
    case 'f':
        return parseFalse(c, v);
    case '\0':
        return ParseError::ExpectValue;
    default:
        return ParseError::InvalidValue;
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
    return ret;
}

} // namespace lept
