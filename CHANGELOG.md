# 更新日志

本项目的全部重要变更记录于此。格式参照 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)：

- **主版本**：语法规范或任一实现的公开 API 发生不兼容变更；
- **次版本**：向后兼容的功能新增（含新增实现）；
- **修订号**：向后兼容的缺陷修复。

整个仓库共用一个版本号：所有实现由同一份通用语料（v0.5.0 起为 60 条）与 dump / demo 双门禁
绑定成一个行为整体，规范与实现必须同步演进，按实现各自定版本没有意义。
每个正式版本用 annotated git tag（`v` 前缀）锚定。

## [0.4.0] - 2026-11-10

本版包含三次破坏性变更（GDScript 与 Go 的集成方式、GDScript 的转义函数
更名），按本仓库的版本策略在 1.0.0 之前以次版本号发布并在此标注。
**默认转义行为对 v0.3.0 向后兼容**（既有转义序列全部不变，只新增
`\uXXXX`），语法规范与其余五套实现的 API 无变更。

### 破坏性变更

- **GDScript：去掉 `class_name Nevext` 全局类名注册，改为 `preload` 引入**。
  旧版脚本首行的 `class_name Nevext` 会把类名注册进 Godot 工程的全局
  命名空间：与使用方工程里的同名类冲突，且同一工程内只能存在一份。
  现在库脚本不再注册全局类名，拷进工程任意目录后以
  `const Nevext := preload("res://<拷贝路径>/nevext.gd")` 引入。
  迁移：在使用了 `Nevext.XXX` 的脚本顶部加一行上述 const 声明即可，
  其余写法不变；`Nevext.null_escape` / `Nevext.nevext_escape` /
  `Nevext.VERSION` 等顶层入口保持可用。
- **Go：库源码移至 module 根目录**（`go/nevext/nevext.go` → `go/nevext.go`，
  工程布局与 stope-lang 的 Go 侧一致），仓库内 import 路径由
  `nevext/nevext` 变为单层 `nevext`，不再出现两层 `nevext`。包名
  （`nevext`）、全部导出标识符与行为不变；单文件拷走的集成方式不受影响
  （import path 本来就由使用方工程决定）。
- **GDScript：`Nevext.defalt_escape` 更名为 `Nevext.nevext_escape`**，
  转义表在原九项（`\'` `\"` `\\` `\n` `\r` `\t` `\v` `\b` `\f`）基础上
  新增 `\uXXXX`（4 位十六进制码点，大小写均可，表项与 json.org 语法图 /
  ECMA-404 的 `\u` 一致）。迁移：把 `defalt_escape(` 改为
  `nevext_escape(` 即可。

### 新增

- **八套实现统一提供三种转义函数并可注入**（此前仅 GDScript 支持）：
  `null_escape`（不转义）/ `nevext_escape`（**默认**：原九项表 +
  `\uXXXX`，对 v0.3.0 及之前**向后兼容**——既有转义序列行为不变，只有
  `\u` 从非法变合法）/ `json_escape`（严格 JSON 表：`\" \\ \/ \b \f \n
  \r \t \uXXXX`，来自 [json.org](https://www.json.org/json-zh.html) 语法图 /
  ECMA-404，不含 `\'` 与 `\v`，供 JSON 互操作校验使用）。各语言按自身
  命名风格导出（`NullEscape` / `nullEscape` 等）。注入点：C++
  `Parser::escapeFunc`、Python `Parser(lexer, escape_func=...)`、Node.js
  `parser.escapeFunc`、C# `parser.EscapeFunc`、Go `parser.EscapeFunc`、
  Rust `parser.escape_func`（或 `Parser::with_escape`）、Lua
  `parser.escape_func`、GDScript 保持历史位置 `lexer.escape_func`。
- 各实现的历史展开入口（`ExpandEscapes` / `expand_escapes` 等）保留为
  `nevext_escape` 的同义词，与 Parser 新默认一致。
- 通用语料由 55 条扩至 60 条：新增 C10~C14 覆盖 `\/`（默认表非法）、
  `\uXXXX` 大小写、`\u` 非法、`\'`（默认表合法——Nevext 有单引号串，
  `\'` 与 `\"` 都需要）。
- C++ 私有套件新增 PAR-06b / PAR-06c（`json_escape` 与 `null_escape`
  注入用例），111 用例 / 796 断言扩至 118 / 832；其余七套各 60 / 449
  扩至 65 / 481（GDScript 为 65 / 478）。

### 变更

- GDScript 不再依赖 `.godot/global_script_class_cache.cfg`：新检出或
  删除 `.godot/` 后无需先做编辑器扫描即可运行测试与 demo
  （无缓存副本实测用例全过）。
- GDScript 词法器用到的常量与转义实现从外层类移入 `Lexer` 内部类
  自包含（Godot 内部类访问外层成员不可靠，去掉全局类名后必须如此），
  外层保留同名静态入口转发，行为与对外 API 不变。
- 文档：GDScript 使用指南补充「Autoload 全局单例」可选集成方式（本体为
  `RefCounted` 不能直接注册，需 `extends Node` 薄包装）与四种加载方式
  （`preload` / `class_name` / Autoload / `load`）的官方定位对照。
- 文档：语法概述重写「转义字符」一节（默认 Nevext 表与严格 JSON 表的
  对照、json.org 来源、代理区码点的跨实现差异 D-9）；API Reference §5
  重写为三函数 × 八实现对照，D-6 更新为「八套均可注入，GDScript 注入点
  在 Lexer（历史），其余在 Parser」。

## [0.3.0] - 2026-11-04

本版包含一次破坏性变更（GDScript 方法更名），按本仓库的版本策略在 1.0.0
之前以次版本号发布并在此标注。语法规范与其余七套实现的 API 无变更。

### 破坏性变更

- **GDScript：`Parser.parse_prog` 更名为 `parse_program`**（与 Python / Lua /
  Rust 的 `parse_program` 及 C++ / C# / Go 的 `ParseProgram` 统一）。
  迁移：调用点把 `parse_prog(` 改为 `parse_program(` 即可，签名与返回值不变。

### 新增

- **[API Reference](./docs/zh-cn/api-reference.md)**：八份实现的接口形状
  对照与差异登记（D-1 ~ D-8），定位「有什么、叫什么、哪里不同」。

## [0.2.0] - 2026-11-04

新增三套实现，测试从六套扩到八套；语法与既有 API 无任何变更，
八套实现行为经通用语料与 dump / demo 双门禁逐字节对拍。

### 新增

- **第六套实现：Lua**（`lua/nevext/nevext.lua`，Lua 5.4+，单文件，零依赖），
  行为逐条对齐 C++ 参考实现；Lua 按字节扫描，详见
  [Lua 使用指南](./docs/zh-cn/guide-lua.md) 与 [Lua 测试计划](./lua/docs/test-plan.md)。
- **第七套实现：Go**（`go/nevext/nevext.go`，Go 1.21+，单文件，零依赖），
  行为逐条对齐 C++ 参考实现；Go 按字节扫描，详见
  [Go 使用指南](./docs/zh-cn/guide-go.md) 与 [Go 测试计划](./go/docs/test-plan.md)。
- **第八套实现：Rust**（`rust/nevext/nevext.rs`，Rust 1.70+，单文件，零依赖），
  行为逐条对齐 C++ 参考实现；Rust 按字节扫描（合法 UTF-8 输入），详见
  [Rust 使用指南](./docs/zh-cn/guide-rust.md) 与 [Rust 测试计划](./rust/docs/test-plan.md)。
- dump / demo 双门禁升级到八实现口径；版本落点增至十三处，
  `tools/check_version.py` 同步扩展。

## [0.1.0] - 2026-11-03

首个版本。语言规范与五套实现的行为契约以 `docs/zh-cn/overview.md` 与
`docs/zh-cn/test-plan.md` 为准；1.0.0 之前语法与 API 仍可能调整，破坏性变更
会升次版本号并在此标注。

### 新增

- **语言规范 v0.1**：`;` 分隔语句、逗号等同空格、`//` 与 `/* */` 注释、
  裸串与引号串、转义在语法层展开；语句解析为字符串列表（见概述文档）。
- **五套实现**，行为逐条对齐、均零第三方依赖、单文件拷走即用：
  - C++（参考实现，header-only 单文件 `nevext.hpp`，C++20）；
  - GDScript（Godot 4.x）；
  - JavaScript（Node.js，ESM）；
  - C#（.NET，`namespace Nevext`）；
  - Python（3.10+，snake_case）。
- **对象复用 API**：`Lexer::Reset` / `Parser::Reset` 及四套实现的对应方法。
- **测试设施**：
  - 跨实现通用语料 55 条（A01~H04），错误按 `(行号, 种类)` 断言；
  - `--dump` 门禁：五套实现的语料结果输出逐字节可比；
  - demo 门禁：五份 `demo01` 同输入同格式，四条路径（内置 / 合法文件 /
    非法转义 / 文件不存在）输出与退出码逐字节可比；
  - 各实现私有套件（C++ 111 用例 / 796 断言，其余各 60 / 449）。
- **版本设施**：版本常量内嵌五套实现源码（`Nevext::kVersion` / `VERSION` /
  `__version__` / `Nevext.Info.Version` / `Nevext.VERSION`），落点一致性由
  `tools/check_version.py` 守护。

[0.4.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.4.0
[0.3.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.3.0
[0.2.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.2.0
[0.1.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.1.0

> AI生成
