# 从零开始的 JSON 库教程（六）：解析对象

本文是《从零开始的 JSON 库教程》的第六个单元。

本单元内容：

1. [JSON 对象](#1-json-对象)
2. [数据结构](#2-数据结构)
3. [重构字符串解析](#3-重构字符串解析)
4. [实现](#4-实现)
5. [总结与练习](#5-总结与练习)

## 1. JSON 对象

此单元是本教程最后一个关于 JSON 解析器的部分。JSON 对象和 JSON 数组非常相似，区别是对象以花括号 `{}` 包裹，由键值对（member）组成，键必须为 JSON 字符串，值是任何 JSON 值，中间以冒号 `:` 分隔。完整语法：

~~~
member = string ws %x3A ws value
object = %x7B ws [ member *( ws %x2C ws member ) ] ws %x7D
~~~

新增错误码：

~~~cpp
enum class ParseError
{
    // ...
    MissKey,                    // 缺少键
    MissColon,                  // 缺少冒号
    MissCommaOrCurlyBracket     // 缺少逗号或右花括号
};
~~~

## 2. 数据结构

要表示键值对的集合，有很多数据结构可供选择：

- **动态数组**：如 `std::vector`，简单，O(1) 索引访问，O(n) 查询
- **有序动态数组**：保证排序，可二分搜寻
- **平衡树**：如 `std::map`，有序遍历，O(log n) 查询
- **哈希表**：如 `std::unordered_map`，平均 O(1) 查询

为简单起见，leptjson 选择用动态数组方案。我们定义一个 `Member` 结构来存储键值对：

~~~cpp
struct Member
{
    std::string key;
    Value value;
};
~~~

然后在 `Value` 中添加：

~~~cpp
class Value
{
public:
    // ...
    size_t objectSize() const
    {
        assert(type_ == Type::Object);
        return object_.size();
    }

    const std::string& objectKey(size_t index) const
    {
        assert(type_ == Type::Object && index < object_.size());
        return object_[index].key;
    }

    const Value& objectValue(size_t index) const
    {
        assert(type_ == Type::Object && index < object_.size());
        return object_[index].value;
    }

    Value& objectValue(size_t index)
    {
        assert(type_ == Type::Object && index < object_.size());
        return object_[index].value;
    }

private:
    // ...
    std::vector<Member> object_;
};
~~~

使用 `std::string` 存储键和 `std::vector<Member>` 存储成员，内存管理自动完成。

## 3. 重构字符串解析

对象的键也是一个 JSON 字符串，但我们需要把解析结果写入 `Member::key` 而不是 `Value::str_`。当前 `parseString()` 直接把结果写入 `Value`，我们可以重构它，分离解析逻辑和结果写入。

提取一个 `parseStringRaw()` 函数，只负责解析字符串并返回结果：

~~~cpp
static ParseError parseStringRaw(Context& c, std::string& result)
{
    // 解析字符串，把结果写入 result
    // 逻辑和 parseString() 一样，只是最后不设置 Value
}
~~~

然后 `parseString()` 变成：

~~~cpp
ParseError Value::parseString(Context& c, Value& v)
{
    std::string result;
    ParseError ret = parseStringRaw(c, result);
    if (ret == ParseError::OK)
    {
        v.str_ = std::move(result);
        v.type_ = Type::String;
    }
    return ret;
}
~~~

解析对象时，可以复用 `parseStringRaw()` 来获取键：

~~~cpp
std::string key;
ParseError ret = parseStringRaw(c, key);
~~~

这种「提取方法（extract method）」是常见的重构手法。有赖于单元测试，我们可以安心重构，确保不破坏现有功能。

## 4. 实现

解析对象与解析数组非常相似，每个迭代多处理一个键和冒号：

~~~cpp
ParseError Value::parseObject(Context& c, Value& v)
{
    assert(!c.json.empty() && c.json.front() == '{');
    c.json.remove_prefix(1); // skip '{'
    skipWhitespace(c);

    if (!c.json.empty() && c.json.front() == '}')
    {
        c.json.remove_prefix(1);
        v.type_ = Type::Object;
        v.object_.clear();
        return ParseError::OK;
    }

    std::string key;
    size_t head = c.stack.size();
    size_t size = 0;

    for (;;)
    {
        // 1. parse key
        if (c.json.empty() || c.json.front() != '"')
        {
            // 释放栈上的成员
            // ...
            return ParseError::MissKey;
        }

        key.clear();
        ParseError ret = parseStringRaw(c, key);
        if (ret != ParseError::OK)
        {
            // 释放栈上的成员
            // ...
            return ret;
        }

        // 2. parse ws colon ws
        skipWhitespace(c);
        if (c.json.empty() || c.json.front() != ':')
        {
            // 释放栈上的成员
            // ...
            return ParseError::MissColon;
        }
        c.json.remove_prefix(1); // skip ':'
        skipWhitespace(c);

        // 3. parse value
        Value val;
        ret = parseValue(c, val);
        if (ret != ParseError::OK)
        {
            // 释放栈上的成员
            // ...
            return ret;
        }

        // 把 member 压栈
        // ...

        skipWhitespace(c);

        // 4. parse ws [comma | right-curly-brace] ws
        if (!c.json.empty() && c.json.front() == ',')
        {
            c.json.remove_prefix(1);
            skipWhitespace(c);
        }
        else if (!c.json.empty() && c.json.front() == '}')
        {
            c.json.remove_prefix(1);
            v.type_ = Type::Object;
            // 从栈弹出成员，设置到 object_
            // ...
            return ParseError::OK;
        }
        else
        {
            // 释放栈上的成员
            // ...
            return ParseError::MissCommaOrCurlyBracket;
        }
    }
}
~~~

在 `parseValue()` 中加入对象分支：

~~~cpp
case '{': return parseObject(c, v);
~~~

## 5. 总结与练习

本单元实现了 JSON 对象的解析，至此你已经完整地实现了一个符合标准的 JSON 解析器。

1. 依第 3 节所述，重构 `parseString()` 为 `parseStringRaw()` + `parseString()`。重构前运行单元测试，重构后确保测试仍通过。
2. 实现 `parseObject()` 中留空的部分。
3. 使用 AddressSanitizer 或 valgrind 检测内存泄漏，确保没有泄漏。
