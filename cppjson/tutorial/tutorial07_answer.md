# 从零开始的 JSON 库教程（七）：生成器解答篇

本文是第七个单元的解答篇。

## 1. 补全 stringifyValue()

完整的 `stringifyValue()`：

~~~cpp
void Value::stringifyValue(std::string& out) const
{
    switch (type_)
    {
    case Type::Null:   out += "null";  break;
    case Type::False:  out += "false"; break;
    case Type::True:   out += "true";  break;
    case Type::Number:
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.17g", number_);
        out += buffer;
        break;
    }
    case Type::String:
        stringifyString(out, str_);
        break;
    case Type::Array:
        out += '[';
        for (size_t i = 0; i < array_.size(); i++)
        {
            if (i > 0) out += ',';
            array_[i].stringifyValue(out);
        }
        out += ']';
        break;
    case Type::Object:
        out += '{';
        for (size_t i = 0; i < object_.size(); i++)
        {
            if (i > 0) out += ',';
            stringifyString(out, object_[i].key);
            out += ':';
            object_[i].value.stringifyValue(out);
        }
        out += '}';
        break;
    }
}
~~~

## 2. Unicode 转义测试

~~~cpp
test_roundtrip("\"Hello\\nWorld\"");
test_roundtrip("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"");
test_roundtrip("\"Hello\\u0020World\"");
~~~

注意 roundtrip 测试中，`\uXXXX` 生成时的十六进制字母大小写必须与输入一致，否则 roundtrip 会失败。如果输入是 `\u0020`，生成时也必须输出 `\u0020`（大写）。

## 3. 性能优化

`std::string` 的 `+=` 每次可能触发重新分配。优化方式：

1. **预计算长度**：对字符串，每个字符最多生成 6 个输出字符（`\u00XX`），加上前后引号，可以预计算 `reserve(len * 6 + 2)`。缺点是对短字符串可能浪费内存。
2. **批量输出**：连续的普通字符可以先扫描一段再整体追加，减少单字符追加次数。缺点是代码更复杂。
3. **使用 `std::string::reserve`**：在 `stringify()` 入口处根据值的类型预估输出长度。缺点是不够精确。

实际应用中，`std::string` 的 `+=` 在现代实现中已经做了很好的动态扩容（通常 1.5 倍或 2 倍增长），对于大多数场景性能已经足够。过度优化的代价是代码复杂度增加，通常不值得。
