# 从零开始的 JSON 库教程（五）：解析数组解答篇

本文是第五个单元的解答篇。

## 1. 单元测试

~~~cpp
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
~~~

## 2. 内存泄漏检测

使用 AddressSanitizer 是最方便的方式。在 CMake 中添加：

~~~cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
~~~

然后编译运行即可自动检测内存泄漏。

使用 valgrind 也可以：

~~~
$ valgrind --leak-check=full ./leptjson_test
~~~

确保 `parseArray()` 在错误路径上正确释放了栈上的 `Value` 对象（调用析构函数），以及在 `Value::free()` 中正确清理了 `array_`。由于 `std::vector` 的析构函数会自动释放所有元素，C++ 版本在内存管理上比 C 版本更容易保证正确性。
