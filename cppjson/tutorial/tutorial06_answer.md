# 从零开始的 JSON 库教程（六）：解析对象解答篇

本文是第六个单元的解答篇。

## 1. 重构 parseStringRaw()

把原来 `parseString()` 中设置 `Value` 的部分改为写入 `std::string& result`：

~~~cpp
static ParseError parseStringRaw(Context& c, std::string& result)
{
    assert(!c.json.empty() && c.json.front() == '"');
    c.json.remove_prefix(1);
    size_t head = c.stack.size();
    // ... 原来的解析逻辑完全不变 ...

    // 成功时：
    result = popString(c, c.stack.size() - head);
    return ParseError::OK;

    // 失败时：
    c.stack.resize(head);
    return ParseError::MissQuotationMark; // 或其他错误
}

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

重构后运行单元测试确认全部通过。

## 2. 实现 parseObject()

关键部分：

**解析键：**
~~~cpp
if (c.json.empty() || c.json.front() != '"')
{
    freeStackMembers(c, size);
    return ParseError::MissKey;
}
std::string key;
ret = parseStringRaw(c, key);
if (ret != ParseError::OK)
{
    freeStackMembers(c, size);
    return ret;
}
~~~

**解析冒号：**
~~~cpp
skipWhitespace(c);
if (c.json.empty() || c.json.front() != ':')
{
    freeStackMembers(c, size);
    return ParseError::MissColon;
}
c.json.remove_prefix(1);
skipWhitespace(c);
~~~

**解析值并压栈：** 使用 `std::vector` 存储临时 `Member`，避免手动管理内存：

~~~cpp
Value val;
ret = parseValue(c, val);
if (ret != ParseError::OK)
{
    freeStackMembers(c, size);
    return ret;
}
// 直接存入临时 vector
members.emplace_back(std::move(key), std::move(val));
size++;
~~~

**遇到 `}` 时完成：**
~~~cpp
v.type_ = Type::Object;
v.object_ = std::move(members);
return ParseError::OK;
~~~

使用 `std::vector<Member>` 作为临时存储，比在原始字节堆栈上做 placement new 更安全、更简洁。

## 3. 内存泄漏

使用 `std::string` 和 `std::vector` 后，析构函数会自动释放内存。只需要确保：

1. 错误路径上临时变量被正确析构（RAII 自动处理）
2. `Value::free()` 中正确清理了 `object_`：

~~~cpp
case Type::Object:
    object_.clear();
    object_.shrink_to_fit();
    break;
~~~
