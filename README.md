# Nevext

Nevext 是一个汇编语言风格的数据描述语言（DDL）。每个语句解析为一个字符串列表，
整个程序解析为语句的列表。

Nevext 只规定**如何把代码字符串解析成列表**，不规定这些列表的语义——
语义完全由使用方自己定义。

“Nevext” 意为 “never extends”，**永远不增加新的语法**。

容易手写实现是 Nevext 的设计目标之一：手写实现容易，用 LLM 生成实现也就容易，
从而方便移植到各种平台。

## 语法简述

```nevext
// 单行注释
windowTitle "Nevext Demo";
windowSize 1280, 720;
/* 多行注释 */
playerName '海上生明月'
```

包含三个语句，解析为 3 字符串数组：

```
["windowTitle", "Nevext Demo"]
["windowSize", "1280", "720"]
["playerName", "海上生明月"]
```

以上示例展示了 Nevext 的几个特点：

- 逗号 `,` 等同空白字符，`1280, 720` 与 `1280 720` 解析结果相同；
- 支持 `//` 单行注释与 `/* */` 多行注释；
- 字符串可以用双引号或单引号，也可以完全不加引号；
- 最后一个语句可以没有分号结尾。

完整的语法规则、转义字符表与边界细节，见 [语法概述](./docs/zh-cn/overview.md)。

## 实现状态

| 语言 | 状态 | 说明 |
| --- | --- | --- |
| C++ | **可用** | header-only，C++20。参考实现，行为以此为准 |
| GDScript | **可用（性能受限）** | 行为已与 C++ 实现逐条对齐，并由同一份语料守卫；但大输入性能明显劣化 |
| JavaScript | **可用** | Node.js，零依赖单文件（ESM）。行为与 C++ 逐条对齐，游标式扫描整体 O(n) |
| C# | **可用** | .NET，零依赖单文件（`namespace Nevext`）。行为与 C++ 逐条对齐，游标式扫描整体 O(n) |
| Python | **可用** | CPython 3.10+，零依赖单文件。行为与 C++ 逐条对齐，游标式扫描整体 O(n) |

五套实现跑同一份跨实现语料（55 条），定义见
[跨实现通用测试计划](./docs/zh-cn/test-plan.md)；各实现另有私有套件，
见 [C++ 测试计划](./cpp/docs/test-plan.md)、
[GDScript 测试计划](./gdscript/docs/test-plan.md)、
[JavaScript 测试计划](./nodejs/docs/test-plan.md)、
[C# 测试计划](./csharp/docs/test-plan.md) 与
[Python 测试计划](./python/docs/test-plan.md)。

## 快速开始

### C++

header-only 的单文件，没有 `.lib` / `.dll`，也没有兄弟头文件：拷 `nevext.hpp` 进工程即可：

```cpp
#include "nevext.hpp"

  // main 函数中
  std::u16string src = u"windowSize 1280, 720;";
  Nevext::Lexer  lexer(src);
  Nevext::Parser parser(lexer);
  Nevext::Program prog;
  auto result = parser.ParseProgram(prog);
```

要点：

- 需要 **C++20**；
- 内部统一使用 `std::u16string`（UTF-16），打印或输出前需转成 UTF-8，
  可参考 `cpp/demo/utf.hpp`（该文件由 LLM 生成）；
- MSVC 必须加 `/utf-8`，否则含中文的 `u"..."` 字面量会被错误解码；
- 更多细节见 [C++ 使用指南](./docs/zh-cn/guide-cpp.md)。

### GDScript

把 `gdscript/nevext/nevext.gd` 拷进 Godot 工程即可，脚本首行的
`class_name Nevext` 会注册为全局类名，无需 `preload`：

```gdscript
var lexer  := Nevext.Lexer.new(src)
var parser := Nevext.Parser.new(lexer)
var res:Nevext.ParseResult = parser.parse_prog()
```

要点：

- 需要 **Godot 4.x**；
- Godot 的 `String` 内部为 UTF-32，中文与 emoji 可直接处理，无需转码；
- 行为与 C++ 实现一致（`TokenType` 枚举项、错误种类与位置、行号统计全部对齐）；
- 每推进一个字符做一次 `substr()`，属超线性量级，大输入请先评估，
  详见 [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md)。

### JavaScript（Node.js）

零依赖单文件（ESM），把 `nodejs/nevext/nevext.js` 拷进工程即可：

```js
import { Lexer, Parser, Result } from './nevext/nevext.js';

const parser = new Parser(new Lexer('windowSize 1280, 720;'));
const res = parser.parseProgram();
if (res.err === Result.Error) console.error(parser.errors);
// res.val -> [{ line: 1, strings: ['windowSize', '1280', '720'] }]
```

要点：

- 需要 **Node.js**（任意近期版本），零第三方依赖；
- JS 字符串即 UTF-16 码元序列，中文与 emoji 可直接处理，无需转码；
- 行为与 C++ 实现一致；语句列表走 `parseProgram()` 的返回值（`{ err, val }`），
  不像 C++ 用出参；
- 采用「完整字符串 + 游标」扫描，整体 O(n)，大输入无码元级性能悬崖；
- 更多细节见 [JavaScript 使用指南](./docs/zh-cn/guide-nodejs.md)。

### C#（.NET）

零依赖单文件，把 `csharp/nevext/Nevext.cs` 拷进工程即可（`namespace Nevext`）：

```csharp
using Nevext;

var parser = new Parser(new Lexer("windowSize 1280, 720;"));
var res = parser.ParseProgram();
if (res.Err == Result.Error) Console.Error.WriteLine(string.Join("\n", parser.Errors));
// res.Val -> [{ Line = 1, Strings = ["windowSize", "1280", "720"] }]
```

要点：

- 需要 **.NET**（本仓库用 SDK 10 验证），零第三方依赖、无 NuGet 引用；
- C# 字符串即 UTF-16 码元序列，中文与 emoji 可直接处理，无需转码；
- 命名与 C++ 参考实现同为 PascalCase；语句列表走 `ParseProgram()` 的返回值
  （`ParseResult.Val`），不像 C++ 用出参；
- 采用「完整字符串 + 游标」扫描，整体 O(n)；
- 更多细节见 [C# 使用指南](./docs/zh-cn/guide-csharp.md)。

### Python

零依赖单文件，把 `python/nevext/nevext.py` 拷进工程即可：

```python
import nevext

parser = nevext.Parser(nevext.Lexer("windowSize 1280, 720;"))
res = parser.parse_program()
if res.err == nevext.Result.Error:
    print("\n".join(parser.errors))
# res.val -> [Statement(line=1, strings=['windowSize', '1280', '720'])]
```

要点：

- 需要 **Python 3.10+**，只用标准库，零第三方依赖，无需安装；
- Python 字符串是码点序列，中文与 emoji 可直接处理，无需转码；
- 命名按 Python 惯例 snake_case；语句列表走 `parse_program()` 的返回值
  （`ParseResult.val`），不像 C++ 用出参；
- 采用「完整字符串 + 游标」扫描，整体 O(n)；
- 更多细节见 [Python 使用指南](./docs/zh-cn/guide-python.md)。

## 示例程序

五套实现各有一份可运行的 `demo01`，使用**同一段输入与同一输出格式**，输出可逐字节比对
（见[跨实现通用测试计划](./docs/zh-cn/test-plan.md) 第 5.1 节的「demo 门禁」）：

```bash
./cpp/build/Debug/demo01.exe      # C++
gdscript\demo.cmd                 # GDScript
node nodejs/demo/demo01.js        # JavaScript
csharp\demo.cmd                   # C#
python python/demo/demo01.py      # Python
```

不带参数时解析内置示例；传入文件路径则解析该文件（UTF-8）。
退出码：`0` 解析成功，`1` 有词法 / 语法错误（分别打印两个错误容器），`2` 文件打不开。

## 测试

```bash
# C++
cd cpp && cmake -B build && cmake --build build && cd build && ctest -C Debug --output-on-failure

# GDScript（工程根目录为 gdscript/；GODOT 可指定 exe）
godot --headless --path gdscript --script res://test/test_main.gd

# JavaScript（Node.js）
node nodejs/test/test_main.js

# C#（.NET）
csharp\test.cmd

# Python
python python/test/test_main.py
```

各套件都支持 `--dump`，按同一行格式打印语料的实际结果，两两 diff 即可显形实现之间的分歧：

```bash
diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(node nodejs/test/test_main.js --dump)

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(godot --headless --path gdscript --script res://test/test_main.gd -- --dump)

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(csharp/bin/Release/net10.0/nevext_test.exe --dump)

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(python python/test/test_main.py --dump)
```

## 文档

- [语法概述](./docs/zh-cn/overview.md) —— 语法规则、转义字符、边界细节
- [C++ 使用指南](./docs/zh-cn/guide-cpp.md) —— 集成、API、错误处理
- [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md) —— 集成、API、已知缺口
- [JavaScript 使用指南](./docs/zh-cn/guide-nodejs.md) —— 集成、API、已知限制
- [C# 使用指南](./docs/zh-cn/guide-csharp.md) —— 集成、API、已知限制
- [Python 使用指南](./docs/zh-cn/guide-python.md) —— 集成、API、已知限制
- [跨实现通用测试计划](./docs/zh-cn/test-plan.md) —— 五套实现共用的语料与断言约定
- [C++ 开发说明](./cpp/docs/development.md) —— 代码格式化配置、目录职责、代码约定、提交前检查清单
- [C++ 测试计划](./cpp/docs/test-plan.md) —— C++ 私有套件（LEX / PAR / ERR）
- [GDScript 测试计划](./gdscript/docs/test-plan.md) —— GDScript 私有套件与 Godot 踩坑清单
- [JavaScript 测试计划](./nodejs/docs/test-plan.md) —— JS 私有套件与实现要点
- [C# 测试计划](./csharp/docs/test-plan.md) —— C# 私有套件与实现要点
- [Python 测试计划](./python/docs/test-plan.md) —— Python 私有套件与实现要点

## 开源协议

[MIT](./LICENSE) 协议。Copyright (c) 2026 nevext contributors。
