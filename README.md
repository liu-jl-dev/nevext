# Nevext

Nevext 是一个汇编语言风格的数据描述语言（DDL）。每个语句解析为一个字符串列表，
整个程序解析为语句的列表。

Nevext 只规定**如何把代码字符串解析成列表**，不规定这些列表的语义——
语义完全由使用方自己定义。

“Nevext” 意为 “never extends”，**永远不增加新的语法**。

容易手写实现是 Nevext 的设计目标之一。手写实现容易，用 LLM 生成实现也就容易，
从而方便移植到各种平台。

如果您在使用文档中产生疑惑，可以提出。这有助于改进文档。

如果您希望将 Stope 移植到任意宿主语言，可以提出 issue。由于 Stope 会保持足够简单，移植会很容易，使用 Agent 进行移植是足够可靠的。

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
| Lua | **可用** | Lua 5.4+，零依赖单文件。行为与 C++ 逐条对齐（字节序列扫描，结构字符全在 ASCII 区间），游标式扫描整体 O(n) |
| Go | **可用** | Go 1.21+，零依赖单文件。行为与 C++ 逐条对齐（字节序列扫描，同 Lua 论证），游标式扫描整体 O(n) |
| Rust | **可用** | Rust 1.70+，零依赖单文件（`Status` 枚举、`Parser` 不绑定词法器）。行为与 C++ 逐条对齐，游标式扫描整体 O(n) |

八套实现跑同一份跨实现语料（55 条），定义见
[跨实现通用测试计划](./docs/zh-cn/test-plan.md)；各实现另有私有套件，
见 [C++ 测试计划](./cpp/docs/test-plan.md)、
[GDScript 测试计划](./gdscript/docs/test-plan.md)、
[JavaScript 测试计划](./nodejs/docs/test-plan.md)、
[C# 测试计划](./csharp/docs/test-plan.md)、
[Python 测试计划](./python/docs/test-plan.md)、
[Lua 测试计划](./lua/docs/test-plan.md)、
[Go 测试计划](./go/docs/test-plan.md) 与
[Rust 测试计划](./rust/docs/test-plan.md)。

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

### Lua

零依赖单文件，把 `lua/nevext/nevext.lua` 拷进工程即可：

```lua
local Nevext = require("nevext")

local lexer = Nevext.Lexer.new("windowSize 1280, 720;")
local parser = Nevext.Parser.new(lexer)
local res = parser:parse_program()
if res.err == Nevext.Result.Error then
  print(table.concat(parser.errors, "\n"))
end
-- res.val -> [{line=1, strings={"windowSize", "1280", "720"}}]
```

要点：

- 需要 **Lua 5.4+**，零第三方依赖，无需安装；
- Lua 字符串是字节序列：本语法的结构字符全在 ASCII 区间，按字节扫描行为
  与按码元扫描完全等价，中文与 emoji 原样保留（元素计数按字节，缺口 LU1）；
- 命名按 Lua 惯例 snake_case，方法用冒号调用（`lexer:next_token()`），
  构造器统一 `Xxx.new(...)`；语句列表走 `parse_program()` 的返回值
  （`{ err, val }`），不像 C++ 用出参；
- 采用「完整字符串 + 游标」扫描，整体 O(n)；
- 更多细节见 [Lua 使用指南](./docs/zh-cn/guide-lua.md)。

### Go

零依赖单文件，把 `go/nevext/nevext.go` 拷进工程即可（`go.mod` 只为仓库内
测试入口存在，拷走时不需要）：

```go
lexer := nevext.NewLexer("windowSize 1280, 720;")
parser := nevext.NewParser(lexer)
res := parser.ParseProgram(true)
if res.Err != nevext.Success {
  for _, e := range parser.Errors {
    fmt.Println(e)
  }
}
// res.Val -> [{1 [windowSize 1280 720]}]
```

要点：

- 需要 **Go 1.21+**，零第三方依赖，无需安装；
- Go 字符串是 UTF-8 字节序列，与 Lua 同款论证：结构字符全在 ASCII 区间，
  按字节扫描行为与按码元扫描完全等价（元素计数按字节，缺口 GO1）；
- 命名按 Go 惯例 PascalCase 导出，错误容器是公开字段（`parser.Errors`）；
- 语句列表走 `ParseProgram` 返回值 `res.Val`；
- 更多细节见 [Go 使用指南](./docs/zh-cn/guide-go.md)。

### Rust

零依赖单文件，把 `rust/nevext/nevext.rs` 拷进工程并在 `Cargo.toml` 里
`[lib] path = "nevext/nevext.rs"`（或复制为 `src/lib.rs`）：

```rust
let mut lexer = Lexer::new("windowSize 1280, 720;");
let mut parser = Parser::new();
let res = parser.parse_program(&mut lexer, true);
if res.err == Status::Error {
  for e in &parser.errors { println!("{}", e); }
}
// res.val -> [Statement { line: 1, strings: ["windowSize", "1280", "720"] }]
```

要点：

- 需要 **Rust 1.70+**（edition 2021），零第三方依赖，无需安装；
- Rust 字符串是 UTF-8 字节序列，与 Lua / Go 同款论证（元素计数按字节，
  缺口 RS1）；`&str` 必须是合法 UTF-8，孤立代理项与非 UTF-8 字节无法表达
  （缺口 RS2，语言限制）；
- 结果枚举叫 `Status`（避免与 `std::result::Result` 混淆）；`Parser` 不绑定
  词法器，`parse_program(&mut lexer, ...)` 每次显式传入；
- 更多细节见 [Rust 使用指南](./docs/zh-cn/guide-rust.md)。

## 示例程序

八套实现各有一份可运行的 `demo01`，使用**同一段输入与同一输出格式**，输出可逐字节比对
（见[跨实现通用测试计划](./docs/zh-cn/test-plan.md) 第 5.1 节的「demo 门禁」）：

```bash
./cpp/build/Debug/demo01.exe      # C++
gdscript\demo.cmd                 # GDScript
node nodejs/demo/demo01.js        # JavaScript
csharp\demo.cmd                   # C#
python python/demo/demo01.py      # Python
lua lua/demo/demo01.lua           # Lua
go\demo.cmd                       # Go
rust\demo.cmd                     # Rust
```

不带参数时解析内置示例；传入文件路径则解析该文件（UTF-8）。
退出码：`0` 解析成功，`1` 有词法 / 语法错误（分别打印两个错误容器），`2` 文件打不开。

## 版本

当前版本 **0.2.0**，遵循[语义化版本](https://semver.org/lang/zh-CN/)：
1.0.0 之前语法规范与各实现的公开 API 仍可能调整（升次版本号），破坏性变更
会在[更新日志](./CHANGELOG.md)标注；1.0.0 起承诺 1.x 内行为与 API 兼容。

- 整个仓库共用一个版本号，正式版本用 annotated tag 锚定（`v` 前缀）；
- 版本常量内嵌在各实现里，拷走后可自报家门：C++ `Nevext::kVersion`、
  JavaScript `VERSION`、Python `__version__`、C# `Nevext.Info.Version`、
  GDScript `Nevext.VERSION`、Lua `Nevext.VERSION`、Go `nevext.Version`、
  Rust `nevext::VERSION`；
- 版本号共有十三个落点（八套源码 + CMake / package.json / csproj /
  Cargo.toml / CHANGELOG），
  一致性由零依赖脚本守护：`python tools/check_version.py`。

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

# Lua（5.4+）
lua lua/test/test_main.lua

# Go（1.21+）
go\test.cmd

# Rust（1.70+）
rust\test.cmd
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

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(lua lua/test/test_main.lua --dump)

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(cd go && go run ./test --dump)

diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(cd rust && cargo run -q --bin nevext_test -- --dump)
```

## 文档

- [语法概述](./docs/zh-cn/overview.md) —— 语法规则、转义字符、边界细节
- [C++ 使用指南](./docs/zh-cn/guide-cpp.md) —— 集成、API、错误处理
- [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md) —— 集成、API、已知缺口
- [JavaScript 使用指南](./docs/zh-cn/guide-nodejs.md) —— 集成、API、已知限制
- [C# 使用指南](./docs/zh-cn/guide-csharp.md) —— 集成、API、已知限制
- [Python 使用指南](./docs/zh-cn/guide-python.md) —— 集成、API、已知限制
- [Lua 使用指南](./docs/zh-cn/guide-lua.md) —— 集成、API、已知限制
- [Go 使用指南](./docs/zh-cn/guide-go.md) —— 集成、API、已知限制
- [Rust 使用指南](./docs/zh-cn/guide-rust.md) —— 集成、API、已知限制
- [跨实现通用测试计划](./docs/zh-cn/test-plan.md) —— 八套实现共用的语料与断言约定
- [C++ 开发说明](./cpp/docs/development.md) —— 代码格式化配置、目录职责、代码约定、提交前检查清单
- [C++ 测试计划](./cpp/docs/test-plan.md) —— C++ 私有套件（LEX / PAR / ERR）
- [GDScript 测试计划](./gdscript/docs/test-plan.md) —— GDScript 私有套件与 Godot 踩坑清单
- [JavaScript 测试计划](./nodejs/docs/test-plan.md) —— JS 私有套件与实现要点
- [C# 测试计划](./csharp/docs/test-plan.md) —— C# 私有套件与实现要点
- [Python 测试计划](./python/docs/test-plan.md) —— Python 私有套件与实现要点
- [Lua 测试计划](./lua/docs/test-plan.md) —— Lua 私有套件与实现要点
- [Go 测试计划](./go/docs/test-plan.md) —— Go 私有套件与实现要点
- [Rust 测试计划](./rust/docs/test-plan.md) —— Rust 私有套件与实现要点

## 开源协议

[MIT](./LICENSE) 协议。Copyright (c) 2026 nevext contributors。
