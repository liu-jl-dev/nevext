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
| C++ | **可用** | header-only，C++20，行为已经过实测验证 |
| GDScript | **实验性，目前并不标准** | 存在与 C++ 实现已确认的行为偏差，请勿用于生产 |

GDScript 版的已知问题（转义会静默丢字符、raw 字符串边界不同、大输入性能劣化）
记录在 [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md) 中。

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
- 该实现目前**并不标准**，详见 [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md)。

## 文档

- [语法概述](./docs/zh-cn/overview.md) —— 语法规则、转义字符、边界细节
- [C++ 使用指南](./docs/zh-cn/guide-cpp.md) —— 集成、API、错误处理
- [GDScript 使用指南](./docs/zh-cn/guide-gdscript.md) —— 集成、API、已知偏差

## 开源协议

[MIT](./LICENSE) 协议。Copyright (c) 2026 nevext contributors。
