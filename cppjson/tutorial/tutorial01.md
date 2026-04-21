# 从零开始的 JSON 库教程（一）：启程

本文是《从零开始的 JSON 库教程》的第一个单元。教程练习源代码位于 `cppjson/` 目录。

本单元内容：

1. [JSON 是什么](#1-json-是什么)
2. [搭建编译环境](#2-搭建编译环境)
3. [头文件与 API 设计](#3-头文件与-api-设计)
4. [JSON 语法子集](#4-json-语法子集)
5. [单元测试](#5-单元测试)
6. [实现解析器](#6-实现解析器)
7. [关于断言](#7-关于断言)
8. [总结与练习](#8-总结与练习)

## 1. JSON 是什么

JSON（JavaScript Object Notation）是一个用于数据交换的文本格式，现时的标准为 [ECMA-404](https://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf)。

虽然 JSON 源自 JavaScript 语言，但它只是一种数据格式，可用于任何编程语言。现时具类似功能的格式有 XML、YAML，当中以 JSON 的语法最为简单。

例如，一个动态网页想从服务器获得数据时，服务器从数据库查找数据，然后把数据转换成 JSON 文本格式：

~~~js
{
    "title": "Design Patterns",
    "subtitle": "Elements of Reusable Object-Oriented Software",
    "author": [
        "Erich Gamma",
        "Richard Helm",
        "Ralph Johnson",
        "John Vlissides"
    ],
    "year": 2009,
    "weight": 1.8,
    "hardcover": true,
    "publisher": {
        "Company": "Pearson Education",
        "Country": "India"
    },
    "website": null
}
~~~

网页的脚本代码就可以把此 JSON 文本解析为内部的数据结构去使用。

从此例子可看出，JSON 是树状结构，而 JSON 只包含 6 种数据类型：

* null: 表示为 null
* boolean: 表示为 true 或 false
* number: 一般的浮点数表示方式，在下一单元详细说明
* string: 表示为 "..."
* array: 表示为 [ ... ]
* object: 表示为 { ... }

我们要实现的 JSON 库，主要是完成 3 个需求：

1. 把 JSON 文本解析为一个树状数据结构（parse）。
2. 提供接口访问该数据结构（access）。
3. 把数据结构转换成 JSON 文本（stringify）。

我们会逐步实现这些需求。在本单元中，我们只实现最简单的 null 和 boolean 解析。

## 2. 搭建编译环境

我们要做的库是跨平台、跨编译器的，同学可使用任意平台进行练习。

练习源代码位于 `cppjson/` 目录。建议同学登记为 GitHub 用户，把项目 fork 一个自己的版本，然后在上面进行修改。不了解版本管理的同学，也可以下载 zip 文件。

我们的 JSON 库名为 leptjson，代码文件只有 3 个：

1. `leptjson.hpp`：leptjson 的头文件，含有命名空间 `lept` 中的类型和类声明。
2. `leptjson.cpp`：leptjson 的实现文件，含有 `Value` 类的成员函数实现。此文件会编译成库。
3. `test.cpp`：我们使用测试驱动开发（test driven development, TDD）。此文件包含测试程序，需要链接 leptjson 库。

为了方便跨平台开发，我们会使用 [CMake](https://cmake.org/)。在 Linux/macOS 下：

~~~
$ cd cppjson
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make
~~~

无论使用什么平台及编译环境，编译运行后会出现：

~~~
$ ./leptjson_test
/path/to/cppjson/test.cpp:76: expect: 3 actual: 0
11/12 (91.67%) passed
~~~

若看到类似以上的结果，说明已成功搭建编译环境，我们可以去看看那几个代码文件的内容了。

## 3. 头文件与 API 设计

### 命名空间

C++ 的命名空间（namespace）可以把类型和函数包裹在一起，避免与其他代码的命名冲突。我们的库使用 `lept` 命名空间：

~~~cpp
namespace lept
{
// 所有类型和类都在这里面
} // namespace lept
~~~

使用时可以通过 `using namespace lept;` 引入，或者显式写出 `lept::Value`、`lept::Type` 等。

### 枚举类型

JSON 中有 6 种数据类型，如果把 true 和 false 当作两个类型就是 7 种。我们使用 `enum class`（作用域枚举）来声明：

~~~cpp
enum class Type
{
    Null,
    False,
    True,
    Number,
    String,
    Array,
    Object
};
~~~

`enum class` 是 C++11 引入的，与普通 `enum` 的关键区别是：枚举值不会隐式转换为整数，也不会与其他枚举冲突。必须通过 `Type::Null` 这样的方式访问，不能直接写 `Null`。

解析过程可能出错，我们也用 `enum class` 定义错误码：

~~~cpp
enum class ParseError
{
    OK = 0,
    ExpectValue,
    InvalidValue,
    RootNotSingular
};
~~~

由于 `enum class` 不能隐式转为 `int`，需要时须使用 `static_cast<int>(Type::Null)` 来转换。

### Value 类

JSON 是一个树形结构，每个节点是一个 JSON 值（JSON value）。我们用 `Value` 类来表示：

~~~cpp
class Value
{
public:
    Value() : type_(Type::Null) {}

    ParseError parse(std::string_view json);
    Type type() const { return type_; }

    bool is_null()   const { return type_ == Type::Null; }
    bool is_false()  const { return type_ == Type::False; }
    bool is_true()   const { return type_ == Type::True; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array()  const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

private:
    Type type_;
};
~~~

在此单元中，我们只需要实现 null、true 和 false 的解析，因此该类只需要存储一个 `Type`。之后的单元会逐步加入其他数据。

`type_` 是私有成员，外部只能通过 `type()` 和 `is_xxx()` 来访问，不能直接修改。类型的修改只能通过 `parse()` 函数间接进行，这样确保了数据的一致性。

用法：

~~~cpp
Value v;                       // 默认构造为 Null
ParseError ret = v.parse("null");  // 解析 JSON
Type t = v.type();             // 获取类型，返回 Type::Null
if (v.is_null()) { /* ... */ } // 便利判断
~~~

### std::string_view

`parse()` 的参数类型是 `std::string_view`，这是 C++17 引入的字符串视图。它是对字符序列的**非拥有引用**——不拷贝数据，只是指向已有的字符串。它可以接受 `const char*`，也可以接受 `std::string`：

~~~cpp
v.parse("null");                    // const char* → string_view，OK
std::string s = "true";
v.parse(s);                         // std::string → string_view，OK
~~~

`string_view` 提供了几个在解析器中非常实用的方法：

| 方法 | 作用 | 等价 C 操作 |
|------|------|------------|
| `sv.empty()` | 是否为空 | `*p == '\0'` |
| `sv.front()` | 取首字符 | `*p` |
| `sv.size()` | 字符数 | 手动计算 |
| `sv.remove_prefix(n)` | 跳过前 n 个字符 | `p += n` |
| `sv.substr(pos, len)` | 取子串视图 | 手动构造 |
| `sv == "null"` | 直接比较 | `strcmp` / 逐字符比较 |

关键点：`substr()` 返回的是新的 `string_view`，不产生内存分配，开销极小。

### Context 内嵌类

解析过程中需要跟踪当前解析到了字符串的哪个位置。我们把解析状态封装为 `Context`，作为 `Value` 的私有内嵌类——因为它只在解析实现中使用，不对外暴露：

~~~cpp
class Value
{
    // ...
private:
    struct Context
    {
        std::string_view json;
        explicit Context(std::string_view j) : json(j) {}
    };
};
~~~

`explicit` 关键字防止隐式转换，确保必须显式构造 `Context` 对象，避免意外的类型转换。

## 4. JSON 语法子集

下面是此单元的 JSON 语法子集，使用 [RFC7159](https://tools.ietf.org/html/rfc7159) 中的 [ABNF](https://tools.ietf.org/html/rfc5234) 表示：

~~~
JSON-text = ws value ws
ws = *(%x20 / %x09 / %x0A / %x0D)
value = null / false / true
null  = "null"
false = "false"
true  = "true"
~~~

当中 `%xhh` 表示以 16 进制表示的字符，`/` 是多选一，`*` 是零或多个，`()` 用于分组。

第一行的意思是，JSON 文本由 3 部分组成，首先是空白（whitespace），接着是一个值，最后是空白。

第二行告诉我们，所谓空白，是由零或多个空格符（space U+0020）、制表符（tab U+0009）、换行符（LF U+000A）、回车符（CR U+000D）所组成。

第三行是说，我们现时的值只可以是 null、false 或 true，它们分别有对应的字面值（literal）。

我们的解析器应能判断输入是否一个合法的 JSON。如果输入的 JSON 不合符这个语法，我们要产生对应的错误码，方便使用者追查问题。

在这个 JSON 语法子集下，我们定义 3 种错误码：

* 若一个 JSON 只含有空白，传回 `ParseError::ExpectValue`。
* 若一个值之后，在空白之后还有其他字符，传回 `ParseError::RootNotSingular`。
* 若值不是那三种字面值，传回 `ParseError::InvalidValue`。

## 5. 单元测试

许多同学在做练习题时，都是以 `printf`/`cout` 打印结果，再用肉眼对比结果是否符合预期。但当软件项目越来越复杂，这个做法会越来越低效。一般我们会采用自动的测试方式，例如单元测试（unit testing）。单元测试也能确保其他人修改代码后，原来的功能维持正确（这称为回归测试／regression testing）。

常用的单元测试框架有 xUnit 系列，如 C++ 的 [Google Test](https://github.com/google/googletest)。我们为了简单起见，会编写一个极简的单元测试类。

### TestRunner 类

我们需要一个能自动比较预期值和实际值，并在不匹配时输出文件名和行号的测试工具。C++20 引入了 `std::source_location`，可以在不使用宏的情况下获取调用点的源码位置：

~~~cpp
#include <source_location>

class TestRunner
{
public:
    template <typename T, typename U>
    void expect_eq(T expect, U actual,
                   std::source_location loc = std::source_location::current())
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
            std::fprintf(stderr, "%s:%u: expect: %d actual: %d\n",
                         loc.file_name(), loc.line(), e, a);
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
~~~

要点：

1. **模板参数** `T` 和 `U`：让 `expect_eq` 可以接受 `ParseError`、`Type` 等枚举类型，无需手动 `static_cast<int>`，转换在函数内部完成。
2. **`std::source_location::current()`** 作为默认参数：它会自动捕获**调用点**的文件名和行号，而不是函数定义处的位置。这样我们就不再需要用宏来获取 `__FILE__` 和 `__LINE__` 了。

### 测试用例

有了 `TestRunner`，写测试就很简洁了：

~~~cpp
static TestRunner runner;

static void test_parse_null()
{
    Value v;
    runner.expect_eq(ParseError::OK, v.parse("null"));
    runner.expect_eq(Type::Null, v.type());
}
~~~

若按照 TDD 的步骤，我们先写一个测试，而 `parse()` 尚未正确实现：

~~~
/path/to/cppjson/test.cpp:44: expect: 0 actual: 1
1/2 (50.00%) passed
~~~

第一个返回 `ParseError::OK`，所以是通过的。第二个测试因为 `parse()` 没有正确设置类型，造成失败。我们再实现 `parse()` 令到它能通过测试。

### 测试驱动开发

一般来说，软件开发是以周期进行的。例如，加入一个功能，再写关于该功能的单元测试。但也有另一种软件开发方法论，称为测试驱动开发（test-driven development, TDD），它的主要循环步骤是：

1. 加入一个测试。
2. 运行所有测试，新的测试应该会失败。
3. 编写实现代码。
4. 运行所有测试，若有测试失败回到 3。
5. 重构代码。
6. 回到 1。

TDD 是先写测试，再实现功能。好处是实现只会刚好满足测试，而不会写了一些不需要的代码，或是没有被测试的代码。

但无论我们是采用 TDD，或是先实现后测试，都应尽量加入足够覆盖率的单元测试。

## 6. 实现解析器

有了 API 的设计、单元测试，终于要实现解析器了。

### 跳过空白

根据语法，JSON 文本的前后都可能有空白。我们实现 `skipWhitespace()` 来跳过空白字符：

~~~cpp
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
~~~

这里使用了 `string_view` 的几个方法：
- `empty()` 检查是否已到末尾
- `front()` 取当前字符
- `remove_prefix(1)` 跳过一个字符（相当于指针后移）

### 解析 null

~~~cpp
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
~~~

`parseNull()` 在被调用时，当前字符一定是 `'n'`（由 `parseValue()` 保证），所以先断言这一点，然后跳过 `'n'`。接下来检查剩余部分是否为 `"ull"`，这里用 `substr(0, 3) != "ull"` 一次性比较三个字符。`string_view` 的 `substr` 返回一个新的视图，零拷贝。

### 解析分派

leptjson 是一个手写的递归下降解析器（recursive descent parser）。由于 JSON 语法特别简单，我们不需要写分词器（tokenizer），只需检测下一个字符，便可以知道它是哪种类型的值，然后调用相关的分析函数。对于完整的 JSON 语法，跳过空白后，只需检测当前字符：

* n ➔ null
* t ➔ true
* f ➔ false
* " ➔ string
* 0-9/- ➔ number
* [ ➔ array
* { ➔ object

所以，我们可以按照 JSON 语法简单翻译成解析函数：

~~~cpp
/* value = null / false / true */
/* 提示：下面代码没处理 false / true，将会是练习之一 */
ParseError Value::parseValue(Context& c, Value& v)
{
    char ch = c.json.empty() ? '\0' : c.json.front();
    switch (ch)
    {
    case 'n':
        return parseNull(c, v);
    /* 练习：加入 't' 和 'f' 的分支 */
    case '\0':
        return ParseError::ExpectValue;
    default:
        return ParseError::InvalidValue;
    }
}
~~~

当字符串为空时，返回 `ParseError::ExpectValue`；首字符不匹配任何已知类型时，返回 `ParseError::InvalidValue`。

### 顶层解析函数

~~~cpp
/* 提示：这里应该是 JSON-text = ws value ws */
/* 以下实现没处理最后的 ws 和 ParseError::RootNotSingular */
ParseError Value::parse(std::string_view json)
{
    Context c(json);
    type_ = Type::Null;
    skipWhitespace(c);
    return parseValue(c, *this);
}
~~~

若 `parse()` 失败，会把 `type_` 设为 `Null`，所以这里先把它设为 `Null`，让 `parseValue()` 写入解析出来的根值。如果解析失败，`type_` 保持 `Null` 不变。

这段代码有意留了一个缺陷：根据语法 `JSON-text = ws value ws`，解析完 value 之后还要跳过空白，并确认没有多余字符。目前的实现没有处理这部分，将会是练习之一。

由于 `skipWhitespace()` 是不会出现错误的，返回类型为 `void`。其它的解析函数会返回 `ParseError`，传递至顶层。

## 7. 关于断言

断言（assertion）是防御式编程的常用手段。C++ 标准库的 [`assert()`](https://en.cppreference.com/w/cpp/error/assert)（需 `#include <cassert>`）提供断言功能：当程序以 release 配置编译时（定义了 `NDEBUG` 宏），`assert()` 不会做检测；而在 debug 配置时，则会在运行时检测条件是否为真，断言失败会直接令程序崩溃。

例如上面的 `parseNull()` 开始时，当前字符应该是 `'n'`，所以我们使用 `assert` 进行断言，确保这个前置条件成立。

初使用断言的同学，可能会错误地把含[副作用](https://en.wikipedia.org/wiki/Side_effect_(computer_science))的代码放在 `assert()` 中：

~~~cpp
assert(x++ == 0); // 这是错误的！
~~~

这样会导致 debug 和 release 版的行为不一样。

另一个问题是，初学者可能会难于分辨何时使用断言，何时处理运行时错误（如返回错误值或抛出异常）。简单的答案是，如果那个错误是由于程序员错误编码所造成的（例如传入不合法的参数），那么应用断言；如果那个错误是程序员无法避免，而是由运行时的环境所造成的，就要处理运行时错误（例如开启文件失败）。

## 8. 总结与练习

本文介绍了如何配置编译环境、单元测试的重要性，以至于一个 JSON 解析器子集的实现。如果你读到这里，还未动手，建议你快点试一下。以下是本单元的练习：

1. 修正关于 `ParseError::RootNotSingular` 的单元测试，若 json 在一个值之后，空白之后还有其它字符，则要返回 `ParseError::RootNotSingular`。
2. 参考 `test_parse_null()`，取消注释并补全 `test_parse_true()`、`test_parse_false()` 单元测试。
3. 参考 `parseNull()` 的实现，取消注释并实现 `parseTrue()` 和 `parseFalse()`，并在 `parseValue()` 中加入 `'t'` 和 `'f'` 的分支。
