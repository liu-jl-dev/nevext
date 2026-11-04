# 更新日志

本项目的全部重要变更记录于此。格式参照 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)：

- **主版本**：语法规范或任一实现的公开 API 发生不兼容变更；
- **次版本**：向后兼容的功能新增（含新增实现）；
- **修订号**：向后兼容的缺陷修复。

整个仓库共用一个版本号：所有实现由同一份 55 条通用语料与 dump / demo 双门禁
绑定成一个行为整体，规范与实现必须同步演进，按实现各自定版本没有意义。
每个正式版本用 annotated git tag（`v` 前缀）锚定。

## [未发布]

### 新增

- **第六套实现：Lua**（`lua/nevext/nevext.lua`，Lua 5.4+，单文件，零依赖），
  行为逐条对齐 C++ 参考实现；Lua 按字节扫描，详见
  [Lua 使用指南](./docs/zh-cn/guide-lua.md) 与 [Lua 测试计划](./lua/docs/test-plan.md)。

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

[0.1.0]: https://github.com/liu-jl-dev/nevext/releases/tag/v0.1.0
