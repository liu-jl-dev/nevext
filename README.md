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

两套实现跑同一份跨实现语料（55 条），定义见
[跨实现通用测试计划](./docs/zh-cn/test-plan.md)；各实现另有私有套件，
见 [C++ 测试计划](./cpp/docs/test-plan.md) 与 [GDScript 测试计划](./gdscript/docs/test-plan.md)。

## 快速开始

### C++

header-only，没有 `.lib` / `.dll`，把目录拷进工程即可：

```cpp
#include "nevext/nevext.hpp"

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

## 测试

```bash
# C++
cd cpp && cmake -B build && cmake --build build && cd build && ctest -C Debug --output-on-failure

# GDScript（工程根目录为 gdscript/；GODOT 可指定 exe）
godot --headless --path gdscript --script res://test/test_main.gd
```

两个套件都支持 `--dump`，按同一行格式打印语料的实际结果，可直接 diff 出实现之间的分歧：

```bash
diff --strip-trailing-cr \
     <(./cpp/build/test/nevext_test --dump) \
     <(godot --headless --path gdscript --script res://test/test_main.gd -- --dump)
```

## 文档

- [语法概述](./docs/zh-cn/overview.md) —— 语法规则、转义字符、边界细节
- [C++ 使用指南](./docs/zh-cn/guide-cpp.md) —— 集成、API、错误处理
- [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md) —— 集成、API、已知缺口
- [跨实现通用测试计划](./docs/zh-cn/test-plan.md) —— 两套实现共用的语料与断言约定
- [C++ 测试计划](./cpp/docs/test-plan.md) —— C++ 私有套件（LEX / PAR / ERR）
- [GDScript 测试计划](./gdscript/docs/test-plan.md) —— GDScript 私有套件与 Godot 踩坑清单

## 开源协议

[MIT](./LICENSE) 协议。Copyright (c) 2026 nevext contributors。
