# Nevext C++ 使用指南

本指南介绍 C++ 实现的集成方式与 API 用法。关于语言本身的语法规则，请先阅读
[概述](./overview.md)。

## 集成步骤
Nevext 的 C++ 实现是 **header-only** 的，没有 `.lib`、`.dll`，把头文件目录拷进工程即可使用。

1. 把 `cpp/nevext/` 整个目录复制到你的工程中；
2. 在需要使用处 `#include "nevext/nevext.hpp"`；
3. 确认工程开启了 C++20（`set(CMAKE_CXX_STANDARD 20)` 或 `/std:c++20`）；

## 环境要求

- C++ 标准：**C++20**。代码使用 `std::u8string` 等 C++20 特性
- 无第三方依赖，只用标准库。但 C++ 标准库没有 utf8 与 utf16 字符串转换工具，需要自行实现，或使用第三方库如 [github.com/nemtrif/utfcpp](https://github.com/nemtrif/utfcpp)。


若你手动集成到 MSVC 工程，请在「项目属性 → C/C++ → 命令行」中加上 `/utf-8`。
GCC / Clang 默认即为 UTF-8，无需额外配置。

## 示例项目
仓库中包含 Windows 下，使用 CMake 的示例项目。[程序入口：cpp/demo/demo01.cpp](../../cpp/demo/demo01.cpp)

### 目录结构
```
cpp/
├── CMakeLists.txt        # 构建 demo01 与单元测试 nevext_test
├── build.cmd             # cmake -B build && cmake --build build
├── test.cmd              # 运行单元测试（ctest）
├── nevext/               # ★ 库本体，只需拷贝这个目录
│   ├── common.hpp        # 枚举定义
│   ├── lexer.hpp         # 词法分析
│   ├── parser.hpp        # 语法分析
│   └── nevext.hpp        # 汇总头，include 这个即可
├── test/                 # 单元测试（零第三方依赖，不随库发布）
│   ├── test_framework.hpp
│   ├── test_util.hpp
│   ├── test_main.cpp
│   ├── test_lexer.cpp
│   ├── test_parser.cpp
│   └── test_error.cpp
└── demo/
    ├── demo01.cpp        # 可运行示例
    └── utf.hpp           # UTF-8 / UTF-16 转换工具（本文件由 LLM 生成）
```

测试计划见 [cpp/docs/test-plan.md](../../cpp/docs/test-plan.md)。

**MSVC 注意事项：** MSVC 默认按 ANSI 代码页解析源文件，含中文的 `u"..."` 字面量会被错误
解码，必须加 `/utf-8` 编译选项。本项目的 `CMakeLists.txt` 已处理：

```cmake
if(MSVC)
    add_compile_options(/utf-8)
endif()
```

## 快速上手

下面是完整可运行的最小示例（取自 `demo/demo01.cpp`）：

```cpp
#include <iostream>
#include <windows.h>
#include "nevext/nevext.hpp"
#include "utf.hpp"

int main() {
  SetConsoleCP(CP_UTF8);         // Windows 控制台切到 UTF-8，否则中文乱码
  SetConsoleOutputCP(CP_UTF8);

  // 注意：Lexer 只接受 std::u16string 左值，且源字符串必须比 Lexer 活得更久
  std::u16string input = u"\
  // 单行注释\n\
  hello \"world\";\n\
  /*            \n\
  多行注释      \n\
  */            \n\
  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾";

  Nevext::Lexer lexer(input);
  Nevext::Parser parser(lexer);
  Nevext::Program prog;
  auto result = parser.ParseProgram(prog);

  if (result) {
    cout << "==== Lexer errors =============================\n";
    for (auto& err : lexer.errors) {
      cout << err << std::endl;
    }
    cout << "==== Parser errors =============================\n";
    for (auto& err : parser.errors) {
      cout << err << std::endl;
    }
    return 1;
  }

  cout << "statements num: " << prog.size() << std::endl;
  for (int i = 0; i < prog.size(); i++) {
    cout << "statement " << i + 1 << " at line " << prog[i].line << ":\n  ";
    for (auto& str : prog[i].strings) {
      string s = utf::Utf16ToUtf8(str);
      cout << s << " ";
    }
    cout << std::endl;
  }
  return 0;
}
```

实际输出：

```
statements num: 2
statement 1 at line 2:
  hello world
statement 2 at line 6:
  海上生明月 天涯共此时
```

## 构建并运行示例

```bat
cd cpp
./build                    :: cmake -B build && cmake --build build
./build/Debug/demo01.exe   :: 运行示例
```

也可以直接调用 CMake：

```bash
cmake -B build
cmake --build build
./build/Debug/demo01.exe
```

运行单元测试（不需要 demo 的 Windows 控制台设置）：

```bash
cd build && ctest -C Debug --output-on-failure
# 或直接跑可执行文件，支持按用例名筛选
./build/test/nevext_test Lexer
```

## API 参考

所有类型和函数都在 `namespace Nevext` 下。

### 枚举（`common.hpp`）
```cpp
enum TokenType {
  Unexpected,   // 词法错误产生的 token
  EndOfFile,
  RawStr,       // 无引号字符串
  QuotedStr,    // 单引号 / 双引号字符串
  Sep,          // 分号 ";"
};

enum Result {
  Success,              // = 0
  Error,                // 一般性错误
  UnknownError,
  UnclosedStringError,
};
```
> **要点：** `Result::Success` 的值是 0，所以判断要用 `if (result)` 表示**出错**，
> 用 `if (result != Nevext::Result::Success)` 更清晰，建议后者。

### Token 与 Lexer（`lexer.hpp`）
```cpp
struct Token {
  int line;                       // 该 token 所在行号（从 1 开始）
  TokenType type;
  std::u16string_view value;      // 视图，不拥有数据
};

struct Lexer {
  int line;                                   // 当前行号
  std::vector<std::string> errors;            // 词法错误
  Lexer(std::u16string& input);               // 只接受非 const 左值引用
  Token NextToken();                          // 取出下一个 token
};
```

一般不需要直接使用 `Lexer`，交给 `Parser` 驱动即可。只有在需要自定义扫描逻辑，
或要读取 `lexer.errors` 时才接触它。

### Parser、Statement 与 Program（`parser.hpp`）

```cpp
struct Statement {
  int line;                             // 语句起始行号
  std::vector<std::u16string> strings;  // 语句解析出的字符串列表
};

using Program = std::vector<Statement>;

struct Parser {
  Lexer* lexer;                         // 裸指针，不拥有所有权
  std::vector<std::string> errors;      // 语法 / 转义错误
  Parser(Lexer& lexer);

  Result ParseProgram(Program& prog, bool pauseWhenError = true);
};
```

**`ParseProgram` 参数**

| 参数 | 说明 |
| --- | --- |
| `prog` | 输出参数。解析结果追加到尾部，函数**不会**清空它；重复调用同一容器会累积 |
| `pauseWhenError` | `true`（默认）：遇到第一个错误立即停止解析；`false`：继续解析，尽量多收集错误 |

无论哪种模式，出错前已经解析出的语句都会保留在 `prog` 中。

## 错误处理

### 错误分成两个容器

| 容器 | 内容 | 典型消息 |
| --- | --- | --- |
| `lexer.errors` | 词法阶段错误 | `line 1: unclosed string literal.` |
| `parser.errors` | 语法 / 转义阶段错误 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

**两个容器互不共享。** 一个未闭合的字符串会同时触发两条错误：词法层报
`unclosed string literal.`（进 `lexer.errors`），语法层因为收到 `Unexpected`
token 再报一条 `unexpected token.`（进 `parser.errors`）。

转义是在语法分析阶段处理的，因此转义错误只在 `parser.errors` 中。

### 不做错误去重

每一处错误都会写进对应的容器，包括同一行内的多处错误；行号相同的错误也会各记一条。

```cpp
// 输入: "a\qb" "c\qd"; e    （pauseWhenError = false）
// parser.errors:
//   line 1: invalid escape character.
//   line 1: invalid escape character.
```

即同一行的两处非法转义会各记一条，不会因为行号相同而被合并。

### 非错误的情况

| 现象 | 说明 |
| --- | --- |
| 未闭合的多行注释 | **不是错误**（语法规定）：从 `/*` 到输入结束全部视为注释，其后的内容会被丢弃，两个错误容器都为空 |
| 末尾语句缺少分号 | 不是错误，语法允许 |
| 空语句 `;` | 不是错误，只是不产生语句 |

### 实测行为

| 输入 | `ParseProgram` 返回 | `prog.size()` | 错误 |
| --- | --- | --- | --- |
| `a; b; c`（末句无分号） | `Success` | 3 | 无 |
| `;;;a;`（空语句） | `Success` | 1 | 无（空语句被丢弃） |
| `abc "unclosed;` | `Error` | 1 | 词法：`unclosed string literal.`<br>语法：`unexpected token.` |
| `abc "bad\q";` | `Error` | 1 | 语法：`invalid escape character.` |

## 字符串编码

### Nevext 解析器内部使用 UTF-16 字符串

Nevext 的所有字符串（`Token::value`、`Statement::strings`、输入）都使用
`std::u16string` / `std::u16string_view`。这样设计的原因是：字符串里的
**元素等价于一个 Unicode 码位层级的可迭代单元**，配合定长两字节便于词法扫描；
代价是非 BMP 字符（emoji 等）会占 **2 个** `char16_t`。

因此：

- 传给 `Lexer` 的输入必须是 `std::u16string`；
- 取出结果若要打印、写文件、走网络，需先转成 UTF-8 的 `std::string`。

`demo/utf.hpp` 就是干这件事的（该文件由 LLM 生成，使用前请自行审阅）。

### 控制台输出

Windows 控制台默认不是 UTF-8，即使转码正确也会显示乱码。`demo/demo01.cpp` 开头有：

```cpp
#include <windows.h>
SetConsoleCP(CP_UTF8);
SetConsoleOutputCP(CP_UTF8);
```

更彻底的做法是切换到 Windows Terminal，或在程序启动后调用
`SetConsoleOutputCP(CP_UTF8)` 并配合支持 UTF-8 的字体。

## 生命周期与常见陷阱

| 陷阱 | 说明 |
| --- | --- |
| **源字符串必须比 Lexer 活得更久** | `Lexer` 保存的是 `std::u16string_view`，不拷贝数据。传入的 `std::u16string` 若先析构，后续解析就是悬垂引用 |
| **不能传临时对象** | 构造函数签名是 `Lexer(std::u16string&)`，非 const 左值引用。`Lexer lexer(u"...")` 无法编译，必须先存成具名变量 |
| **Parser 只持有 Lexer 指针** | `Parser` 内部是 `Lexer*`，不拥有所有权。确保 `Lexer` 生命周期覆盖 `Parser` |
| **Token::value 是视图** | `u16string_view` 指向源字符串，源串失效后不可再用 |
| **prog 不会被清空** | `ParseProgram` 是追加语义。复用同一个 `Program` 对象解析多份输入前，记得 `prog.clear()` |
| **emoji 占两个单元** | `U+1F315` 在 `std::u16string` 中长度为 2，做下标运算时要注意 |
| **逗号等同空格** | `a, b` 与 `a b` 解析结果相同，这是语法规定而非 bug |

## 许可

本项目使用 MIT 协议，详见仓库根目录的 [LICENSE](../../LICENSE)。
