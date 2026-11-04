# 更新日志

本项目的全部重要变更记录于此。格式参照 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)：

- **主版本**：语法规范或任一实现的公开 API 发生不兼容变更；
- **次版本**：向后兼容的功能新增（含新增实现）；
- **修订号**：向后兼容的缺陷修复。

整个仓库共用一个版本号：所有实现由同一份 55 条通用语料与 dump / demo 双门禁
绑定成一个行为整体，规范与实现必须同步演进，按实现各自定版本没有意义。
每个正式版本用 annotated git tag（`v` 前缀）锚定。

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

[0.3.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.3.0
[0.2.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.2.0
[0.1.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.1.0
