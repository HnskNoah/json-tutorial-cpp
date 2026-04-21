# 从零开始的 JSON 库教程（八）：访问与其他功能

本文是《从零开始的 JSON 库教程》的第八个单元。

本单元内容：

1. [对象键值查询](#1-对象键值查询)
2. [相等比较](#2-相等比较)
3. [复制与移动](#3-复制与移动)
4. [动态数组](#4-动态数组)
5. [动态对象](#5-动态对象)
6. [总结与练习](#6-总结与练习)

## 1. 对象键值查询

在实际使用中，我们许多时候需要查询一个键是否存在，如存在则获取其值。我们可以提供一个函数，用线性搜寻实现 O(n) 查询：

~~~cpp
const Value* Value::findObjectValue(std::string_view key) const
{
    assert(type_ == Type::Object);
    for (size_t i = 0; i < object_.size(); i++)
    {
        if (object_[i].key == key)
            return &object_[i].value;
    }
    return nullptr;
}

Value* Value::findObjectValue(std::string_view key)
{
    assert(type_ == Type::Object);
    for (size_t i = 0; i < object_.size(); i++)
    {
        if (object_[i].key == key)
            return &object_[i].value;
    }
    return nullptr;
}
~~~

使用：

~~~cpp
Value v;
v.parse("{\"name\":\"Milo\", \"gender\":\"M\"}");
if (auto* val = v.findObjectValue("name"))
    std::cout << val->string() << "\n";
~~~

`std::string` 可以和 `std::string_view` 直接比较，无需手动传长度。

## 2. 相等比较

为了测试结果的正确性，我们实现 `Value` 的相等比较。首先，两个值的类型必须相同，对于 true、false、null 只比较类型即可。对于数字和字符串，需进一步比较值：

~~~cpp
bool Value::operator==(const Value& rhs) const
{
    if (type_ != rhs.type_)
        return false;
    switch (type_)
    {
    case Type::String:
        return str_ == rhs.str_;
    case Type::Number:
        return number_ == rhs.number_;
    case Type::Array:
        if (array_.size() != rhs.array_.size())
            return false;
        for (size_t i = 0; i < array_.size(); i++)
            if (!(array_[i] == rhs.array_[i]))
                return false;
        return true;
    case Type::Object:
        // 对象的键值对是无序的，留作练习
        return false;
    default:
        return true;
    }
}
~~~

数组的比较通过递归实现。对象的比较更复杂，因为 `{"a":1,"b":2}` 和 `{"b":2,"a":1}` 虽然键值次序不同，但它们是相等的。

## 3. 复制与移动

C++ 天然支持复制和移动语义。`Value` 包含 `std::string` 和 `std::vector`，它们的复制构造函数会执行深拷贝，移动构造函数会转移资源。

然而，由于 `Value` 包含 `Type` 和多种数据成员，默认的复制/移动行为可能不正确（例如复制时 `type_` 和实际数据不匹配）。我们需要自定义：

~~~cpp
class Value
{
public:
    Value() : type_(Type::Null), number_(0) {}
    ~Value() { free(); }

    // 复制构造
    Value(const Value& rhs) : type_(Type::Null), number_(0)
    {
        copyFrom(rhs);
    }

    // 复制赋值
    Value& operator=(const Value& rhs)
    {
        if (this != &rhs)
        {
            free();
            copyFrom(rhs);
        }
        return *this;
    }

    // 移动构造
    Value(Value&& rhs) noexcept : type_(rhs.type_), number_(rhs.number_),
        str_(std::move(rhs.str_)), array_(std::move(rhs.array_)),
        object_(std::move(rhs.object_))
    {
        rhs.type_ = Type::Null;
    }

    // 移动赋值
    Value& operator=(Value&& rhs) noexcept
    {
        if (this != &rhs)
        {
            free();
            type_ = rhs.type_;
            number_ = rhs.number_;
            str_ = std::move(rhs.str_);
            array_ = std::move(rhs.array_);
            object_ = std::move(rhs.object_);
            rhs.type_ = Type::Null;
        }
        return *this;
    }

private:
    void copyFrom(const Value& rhs)
    {
        type_ = rhs.type_;
        number_ = rhs.number_;
        str_ = rhs.str_;
        array_ = rhs.array_;
        object_ = rhs.object_;
    }
};
~~~

移动语义在 C++ 中是语言层面支持的，不需要像 C 那样通过不同名字的函数来实现复制和移动。

`std::swap()` 默认就使用移动语义，可以高效交换两个 `Value`：

~~~cpp
Value a, b;
std::swap(a, b);  // O(1) 交换，只移动指针
~~~

## 4. 动态数组

在此单元之前，每个数组的元素数目在解析后是固定不变的。我们添加修改数组的接口：

~~~cpp
class Value
{
public:
    // ...
    // 数组操作
    Value& pushBackArrayElement()
    {
        assert(type_ == Type::Array);
        array_.emplace_back();
        return array_.back();
    }

    void popBackArrayElement()
    {
        assert(type_ == Type::Array && !array_.empty());
        array_.pop_back();
    }

    Value& insertArrayElement(size_t index)
    {
        assert(type_ == Type::Array && index <= array_.size());
        array_.emplace(array_.begin() + index);
        return array_[index];
    }

    void eraseArrayElement(size_t index, size_t count)
    {
        assert(type_ == Type::Array && index + count <= array_.size());
        array_.erase(array_.begin() + index, array_.begin() + index + count);
    }

    void clearArray()
    {
        assert(type_ == Type::Array);
        array_.clear();
    }
};
~~~

`std::vector` 提供了 `emplace_back`、`pop_back`、`emplace`、`erase`、`clear` 等方法，我们只需要简单包装即可。

容量管理也可以暴露：

~~~cpp
size_t arrayCapacity() const
{
    assert(type_ == Type::Array);
    return array_.capacity();
}

void reserveArray(size_t capacity)
{
    assert(type_ == Type::Array);
    array_.reserve(capacity);
}

void shrinkArrayToFit()
{
    assert(type_ == Type::Array);
    array_.shrink_to_fit();
}
~~~

## 5. 动态对象

对象也添加类似的修改接口：

~~~cpp
class Value
{
public:
    // ...
    // 对象操作
    Value& setObjectValue(std::string_view key)
    {
        assert(type_ == Type::Object);
        // 先搜索是否存在
        for (size_t i = 0; i < object_.size(); i++)
        {
            if (object_[i].key == key)
                return object_[i].value;
        }
        // 不存在则新增
        object_.emplace_back(std::string(key), Value());
        return object_.back().value;
    }

    void removeObjectValue(size_t index)
    {
        assert(type_ == Type::Object && index < object_.size());
        object_.erase(object_.begin() + index);
    }

    void clearObject()
    {
        assert(type_ == Type::Object);
        object_.clear();
    }

    size_t objectCapacity() const
    {
        assert(type_ == Type::Object);
        return object_.capacity();
    }

    void reserveObject(size_t capacity)
    {
        assert(type_ == Type::Object);
        object_.reserve(capacity);
    }

    void shrinkObjectToFit()
    {
        assert(type_ == Type::Object);
        object_.shrink_to_fit();
    }
};
~~~

`setObjectValue()` 先搜索是否存在该键，存在则返回该值的引用，不存在则新增。这样调用方可以用 `v.setObjectValue("name").setString("Milo")` 来设置键值。

## 6. 总结与练习

本单元加入了数组和对象的访问、修改方法。C++ 的移动语义、RAII 和标准容器使得这些功能的实现比 C 版本简洁得多。

1. 完成 `operator==` 中对象比较的部分。对象的键值对是无序的，需要逐一查找匹配。
2. 实现上述所有动态数组和动态对象的接口，编写单元测试。
3. 使用 `std::move` 优化 `insertArrayElement()` 和 `setObjectValue()` 中的资源转移。
