# 从零开始的 JSON 库教程（五）：解析数组

本文是《从零开始的 JSON 库教程》的第五个单元。

本单元内容：

1. [JSON 数组](#1-json-数组)
2. [数据结构](#2-数据结构)
3. [解析过程](#3-解析过程)
4. [实现](#4-实现)
5. [总结与练习](#5-总结与练习)

## 1. JSON 数组

从零到这第五单元，我们终于要解析一个 JSON 的复合数据类型了。一个 JSON 数组可以包含零至多个元素，而这些元素也可以是数组类型，可以表示嵌套的数据结构。先看看 JSON 数组的语法：

~~~
array = %x5B ws [ value *( ws %x2C ws value ) ] ws %x5D
~~~

当中 `%x5B` 是 `[`，`%x2C` 是 `,`，`%x5D` 是 `]`。一个数组可以包含零至多个值，以逗号分隔，例如 `[]`、`[1,2,true]`、`[[1,2],[3,4],"abc"]` 都是合法的数组。注意 JSON 不接受末端额外的逗号，例如 `[1,2,]` 是不合法的。

新增错误码：

~~~cpp
enum class ParseError
{
    // ...
    MissCommaOrSquareBracket  // 缺少逗号或右方括号
};
~~~

## 2. 数据结构

JSON 数组存储零至多个元素，最简单的选择是使用 C++ 的 `std::vector<Value>`。`std::vector` 是动态数组，能以 O(1) 用索引访问任意元素，且内存布局紧凑。

我们为 `Value` 类添加数组存储：

~~~cpp
class Value
{
public:
    // ...
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

private:
    Type type_;
    double number_;
    std::string str_;
    std::vector<Value> array_;
};
~~~

使用 `std::vector<Value>` 的好处：
- 自动管理内存的分配和释放
- `push_back()` 可以动态添加元素
- `operator[]` 提供 O(1) 索引访问
- 析构函数自动释放所有元素

`free()` 函数也需要更新：

~~~cpp
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
~~~

## 3. 解析过程

在解析字符串时，我们使用了一个动态堆栈作为临时缓冲区。对于 JSON 数组，我们可以用相同的方法：把每个解析好的元素压入堆栈，解析到数组结束时，再一次性把所有元素弹出，复制到 `std::vector` 中。

但和字符串不同，JSON 数组是中间节点，可能包含嵌套结构。只要在解析函数结束时还原堆栈的状态，就没有问题。

我们用 `["abc",[1,2],3]` 的解析过程来说明：

1. 遇到 `[`，进入 `parseArray()`
2. 遇到 `"abc"`，调用 `parseString()`，把字符压栈，解析完后弹出字符，生成字符串值
3. 把临时元素压栈
4. 遇到 `[1,2]`，进入另一个 `parseArray()`，解析两个数字，生成数组值
5. 把该数组压栈
6. 遇到 `3`，解析数字，压栈
7. 遇到 `]`，弹出 3 个元素，生成数组

由于每次进入子解析函数时都会恢复堆栈状态，嵌套解析不会互相干扰。

## 4. 实现

~~~cpp
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

    size_t head = c.stack.size();
    size_t size = 0;

    for (;;)
    {
        // 解析一个值到临时变量
        Value e;
        ParseError ret = parseValue(c, e);
        if (ret != ParseError::OK)
        {
            // 释放栈上的临时值
            for (size_t i = 0; i < size; i++)
            {
                auto* pv = reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ret;
        }

        // 把临时值压栈
        size_t oldSize = c.stack.size();
        c.stack.resize(oldSize + sizeof(Value));
        new (c.stack.data() + oldSize) Value(std::move(e));
        size++;

        skipWhitespace(c);

        if (!c.json.empty() && c.json.front() == ',')
        {
            c.json.remove_prefix(1);
            skipWhitespace(c);
        }
        else if (!c.json.empty() && c.json.front() == ']')
        {
            c.json.remove_prefix(1);
            v.type_ = Type::Array;
            v.array_.resize(size);
            // 从栈中弹出元素
            for (size_t i = size; i > 0; i--)
            {
                auto* pv = reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                v.array_[i - 1] = std::move(*pv);
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ParseError::OK;
        }
        else
        {
            // 释放栈上的临时值
            for (size_t i = 0; i < size; i++)
            {
                auto* pv = reinterpret_cast<Value*>(c.stack.data() + c.stack.size() - sizeof(Value));
                pv->~Value();
                c.stack.resize(c.stack.size() - sizeof(Value));
            }
            return ParseError::MissCommaOrSquareBracket;
        }
    }
}
~~~

注意几点：
1. 使用 placement `new` 在堆栈上构造 `Value`，使用 `std::move` 避免不必要的复制
2. 错误时需要手动调用析构函数释放栈上的 `Value` 对象
3. 成功时从栈弹出元素，逆序移动到 `array_` 中

在 `parseValue()` 中加入数组分支：

~~~cpp
case '[': return parseArray(c, v);
~~~

## 5. 总结与练习

本单元实现了 JSON 数组的解析，利用 `std::vector<Value>` 管理动态数组，避免了手动内存管理的复杂性。

1. 编写 `test_parse_array()` 单元测试，解析以下两个 JSON：

~~~js
[ null , false , true , 123 , "abc" ]
[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]
~~~

2. 确保解析错误时没有内存泄漏。可以使用 AddressSanitizer（编译选项 `-fsanitize=address`）或 valgrind 检测。
