# 从零开始的 JSON 库教程（八）：访问与其他功能解答篇

本文是第八个单元的解答篇。

## 1. 对象相等比较

对象的键值对是无序的，所以需要逐一查找匹配：

~~~cpp
case Type::Object:
    if (object_.size() != rhs.object_.size())
        return false;
    for (size_t i = 0; i < object_.size(); i++)
    {
        const Value* v = rhs.findObjectValue(object_[i].key);
        if (v == nullptr || !(object_[i].value == *v))
            return false;
    }
    return true;
~~~

对于每个键值对，在另一个对象中查找相同键的值，然后递归比较。不需要考虑重复键的情况。

## 2. 动态接口实现

由于使用了 `std::vector`，大部分接口只是简单包装：

~~~cpp
// 数组
Value& Value::pushBackArrayElement()
{
    assert(type_ == Type::Array);
    array_.emplace_back();
    return array_.back();
}

void Value::popBackArrayElement()
{
    assert(type_ == Type::Array && !array_.empty());
    array_.pop_back();
}

Value& Value::insertArrayElement(size_t index)
{
    assert(type_ == Type::Array && index <= array_.size());
    array_.emplace(array_.begin() + index);
    return array_[index];
}

void Value::eraseArrayElement(size_t index, size_t count)
{
    assert(type_ == Type::Array && index + count <= array_.size());
    array_.erase(array_.begin() + index, array_.begin() + index + count);
}

void Value::clearArray()
{
    assert(type_ == Type::Array);
    array_.clear();
}

// 对象
Value& Value::setObjectValue(std::string_view key)
{
    assert(type_ == Type::Object);
    for (size_t i = 0; i < object_.size(); i++)
    {
        if (object_[i].key == key)
            return object_[i].value;
    }
    object_.emplace_back(std::string(key), Value());
    return object_.back().value;
}

void Value::removeObjectValue(size_t index)
{
    assert(type_ == Type::Object && index < object_.size());
    object_.erase(object_.begin() + index);
}

void Value::clearObject()
{
    assert(type_ == Type::Object);
    object_.clear();
}
~~~

## 3. 移动语义优化

在 `insertArrayElement()` 中，`emplace` 会默认构造一个 `Value`，可能不是最理想的。可以通过右值引用版本支持移动插入：

~~~cpp
Value& insertArrayElement(size_t index, Value&& val)
{
    assert(type_ == Type::Array && index <= array_.size());
    array_.emplace(array_.begin() + index, std::move(val));
    return array_[index];
}
~~~

类似地，`setObjectValue()` 也可以提供接受右值的重载：

~~~cpp
Value& setObjectValue(std::string_view key, Value&& val)
{
    assert(type_ == Type::Object);
    for (size_t i = 0; i < object_.size(); i++)
    {
        if (object_[i].key == key)
        {
            object_[i].value = std::move(val);
            return object_[i].value;
        }
    }
    object_.emplace_back(std::string(key), std::move(val));
    return object_.back().value;
}
~~~

使用移动语义可以避免深拷贝，提高性能。例如：

~~~cpp
Value v;
v.parse("{}");
Value temp;
temp.setString("Hello");
v.setObjectValue("name", std::move(temp));  // temp 被移动，不再有效
~~~
