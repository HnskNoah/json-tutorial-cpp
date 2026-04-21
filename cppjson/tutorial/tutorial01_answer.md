# 从零开始的 JSON 库教程（一）：启程解答篇

本文是第一个单元的解答篇。

## 1. 修正 ParseError::RootNotSingular

我们从 JSON 语法发现，JSON 文本应该有 3 部分：

~~~
JSON-text = ws value ws
~~~

但原来的 `parse()` 只处理了前两部分。我们只需要加入第三部分，解析空白，然后检查 JSON 文本是否完结：

~~~cpp
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
            ret = ParseError::RootNotSingular;
    }
    return ret;
}
~~~

解析成功后，再跳过空白，用 `!c.json.empty()` 检查是否还有多余字符。如果有，说明根值后还有内容，返回 `ParseError::RootNotSingular`。

有一些 JSON 解析器完整解析一个值之后就会顺利返回，这是不符合标准的。但有时候也有另一种需求，文本中含多个 JSON 或其他文本串接在一起，希望当完整解析一个值之后就停下来。因此，有一些 JSON 解析器会提供这种选项，例如 RapidJSON 的 `kParseStopWhenDoneFlag`。

## 2. true/false 单元测试

参考 `test_parse_null()`，取消注释并补全 `test_parse_true()` 和 `test_parse_false()`：

~~~cpp
static void test_parse_true()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("true"));
    runner.expect_eq(Type::True, v.type());
}

static void test_parse_false()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("false"));
    runner.expect_eq(Type::False, v.type());
}
~~~

然后在 `test_parse()` 中取消注释对这两个函数的调用：

~~~cpp
static void test_parse()
{
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
}
~~~

## 3. true/false 解析

参考 `parseNull()`，实现 `parseTrue()` 和 `parseFalse()`：

~~~cpp
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
~~~

然后在 `parseValue()` 中加入 `'t'` 和 `'f'` 的分支：

~~~cpp
ParseError Value::parseValue(Context& c, Value& v)
{
    char ch = c.json.empty() ? '\0' : c.json.front();
    switch (ch)
    {
    case 'n':
        return parseNull(c, v);
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
~~~

### 进一步优化

这 3 种类型都是解析字面值（literal），可以抽取为一个通用函数 `parseLiteral()`：

~~~cpp
ParseError Value::parseLiteral(Context& c, Value& v, std::string_view literal, Type type)
{
    if (c.json.size() < literal.size() || c.json.substr(0, literal.size()) != literal)
        return ParseError::InvalidValue;
    c.json.remove_prefix(literal.size());
    v.type_ = type;
    return ParseError::OK;
}
~~~

然后在 `parseValue()` 中调用：

~~~cpp
case 'n': return parseLiteral(c, v, "null", Type::Null);
case 't': return parseLiteral(c, v, "true", Type::True);
case 'f': return parseLiteral(c, v, "false", Type::False);
~~~

`std::string_view` 作为参数可以直接接受字符串字面量（如 `"null"`），`substr` 和比较操作零拷贝，简洁且高效。
