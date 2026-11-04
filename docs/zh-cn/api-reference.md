# API Reference（八份实现的接口形状对照）

> 定位：**有什么、叫什么、哪里不同** —— 按类型给出八份实现的成员对照。
> 怎么用（示例、错误文案、排错）在各实现的 [使用指南](#8-相关文件)；
> 行为契约（55 条语料、断言约定、双门禁）在
> [跨实现通用测试计划](test-plan.md)，本文不复述语义。
>
> 命名只是风格：PascalCase（C++ / C# / Go）、camelCase（Node.js）与
> snake_case（GDScript / Python / Lua / Rust）。**能力集合八套一致**；
> 真正的差异集中在 §7「差异登记」。

## 0. 入口对照

| 实现 | 单文件入口 | 访问方式 | 版本常量 |
| --- | --- | --- | --- |
| C++ | `cpp/nevext/nevext.hpp`（header-only） | `namespace Nevext` | `Nevext::kVersion`（`string_view`） |
| GDScript | `gdscript/nevext/nevext.gd` | 全局类 `Nevext`，类型是它的内部类（`Nevext.Lexer`…） | `Nevext.VERSION` |
| Node.js | `nodejs/nevext/nevext.js` | ESM 具名导出（`TokenType` / `Lexer`…） | `VERSION` |
| C# | `csharp/nevext/Nevext.cs` | `namespace Nevext` | `Nevext.Info.Version` |
| Python | `python/nevext/nevext.py` | 单模块（`from nevext import Lexer, Parser`） | `__version__` |
| Lua | `lua/nevext/nevext.lua` | `require("nevext")` → 表 `Nevext` | `Nevext.VERSION` |
| Go | `go/nevext/nevext.go` | 包 `nevext`（拷走后自建 module 亦可） | `nevext.Version` |
| Rust | `rust/nevext/nevext.rs` | crate 根扁平导出（`nevext::Lexer`…） | `nevext::VERSION` |

## 1. 枚举与常量

**TokenType** —— 取值与顺序八套一致（0 起）：

| 值 | 含义 |
| --- | --- |
| `Unexpected` (0) | 非法 token（未闭合引号串、无法推进的字符等） |
| `EndOfFile` (1) | 输入结束 |
| `RawStr` (2) | 未加引号的裸串（不展开转义） |
| `QuotedStr` (3) | 引号串（literal 保留转义序列原文，语法层再展开） |
| `Sep` (4) | 分号 |

**Result** —— 解析结果码，`Success = 0`（可直接写真值判断）：

| 成员 | 对照 |
| --- | --- |
| 状态码 | `Success = 0`、`Error = 1`，八套一致；Rust 的枚举名叫 **`Status`**（避开 `std::result::Result`） |
| 容器形态 | JS / C# / Python / Lua / Go / Rust：`Result` 只是状态码，语句列表放独立的 `ParseResult`；GDScript 的 `Result` 是**类**（`{err, val}`，`OK=0` / `Err=1`），同时充当解析结果容器 |
| C++ 内部值 | C++ 的 `Result` 额外有 `UnknownError` / `UnclosedStringError`，只作 `scanQuotedStr` 的内部返回值；对外经 `NextToken` 一律表现为 `Unexpected` token + errors（见 D-1） |

**错误消息常量** —— 三条文本逐字一致（含句号后的空格），哪些实现公开为常量：

| 消息 | 公开为常量的实现 | 内联字面量的实现 |
| --- | --- | --- |
| `"unclosed string literal. "` | C# `Lexer.UnclosedMessage` · Go `UnclosedMessage` · Rust `UNCLOSED_MESSAGE` · Python 模块级 `UNCLOSED_MESSAGE` · Lua `Nevext.UNCLOSED_MESSAGE` | C++ · Node.js · GDScript |
| `"invalid escape character. "` | 同上（`InvalidEscapeMessage` / `INVALID_ESCAPE_MESSAGE`） | 同上 |
| `"unexpected token. "` | 同上（`UnexpectedMessage` / `UNEXPECTED_MESSAGE`） | 同上 |

版本常量见 §0 末列；十三处落点的一致性由 `tools/check_version.py` 守护。

## 2. Token / Statement / Program

| 类型 | 成员 | 对照 |
| --- | --- | --- |
| **Token** | 类型 | `TokenType` 字段：C++ `type` · 其余 `type` · **Rust 叫 `kind`**（`type` 是保留字） |
| | 行号 | `line`（C++ `int`，Go / Rust `int` / `usize`） |
| | 原文 | C++ `value`（`u16string_view`，**视图**，生命周期绑定调用方持有的输入串）· 其余七套 `literal`（拥有所有权的字符串；EOF / Unexpected 时为空串，GDScript 例外见 D-5） |
| **Statement** | 成员 | `line`（语句起始物理行号）+ `strings`（转义展开后的字符串列表）。容器：C++ `vector<u16string>` · GDScript `Array[String]` · C# `List<string>` · Go `[]string` · Rust `Vec<String>` · 其余语言原生数组 |
| **Program** | 形态 | C++ 有类型别名 `Program = std::vector<Statement>`，`ParseProgram` 经出参填充；其余七套无独立类型，就是 `ParseResult.val` 里的列表 |

## 3. Lexer

| 成员 | 对照 |
| --- | --- |
| 类型 | `Nevext::Lexer` · `Nevext.Lexer` · `Lexer` · `Nevext.Lexer` · `nevext.Lexer` · `Nevext.Lexer` · `nevext.Lexer` · `nevext::Lexer` |
| 构造 | 全部收源码字符串：`Lexer(input)` / `Nevext.Lexer.new(code)` / `new Lexer(input)` / `NewLexer(code)` / `Lexer::new(code)`；C++ 收 `std::u16string&`（外部 UTF-8 → UTF-16 由调用方转换，示例 `cpp/demo/utf.hpp`） |
| 取下一 token | `NextToken()`（C++ / C# / Go）· `nextToken()`（Node.js）· `next_token()`（GDScript / Python / Lua / Rust），返回 `Token` |
| 重置 | `Reset(input)`（Pascal 组）· `reset(code)`（snake 组）—— 行号归 1、清 errors、游标归零；八套语义一致，支持对象复用 |
| 行号 | 公开字段 `line`（1 起，跨行引号串与被转义的换行都计入） |
| 错误容器 | 公开字段 `errors`，格式 `line N: msg`；**只收词法错误**（未闭合字符串），语法错误在 `parser.errors` |
| 输入持有 | C++ `input`（视图）· GDScript `code` · Node.js `input` · Python / Lua / Rust `code` · Go `Code` + `Pos`（游标公开）· Rust `code` + `pos`（公开）；C# 游标与原文私有 |
| 转义注入 | **仅 GDScript**：字段 `escape_func: Callable`（默认 `Nevext.defalt_escape`，拼写如此、是公开 API 名），由 Parser 调用、只作用于 QuotedStr；其余七套转义表内建（见 §5） |

## 4. Parser

| 成员 | 对照 |
| --- | --- |
| 类型 | `Nevext::Parser` · `Nevext.Parser` · `Parser` · `Nevext.Parser` · `nevext.Parser` · `Nevext.Parser` · `nevext.Parser` · `nevext::Parser` |
| 构造与绑定 | 七套构造时绑定词法器并可在 `Reset(lexer)` 换绑；**Rust 例外**：`Parser::new()` 不绑定，`parse_program(&mut lexer, ...)` 每次显式传参（避免自引用结构，见 D-4），`reset()` 只清错误 |
| 解析 | `ParseProgram(prog&, pauseWhenError=true)`（C++，**出参**）· `ParseProgram(pauseWhenError=true)`（C# / Go）· `parseProgram(pauseWhenError=true)`（Node.js）· `parse_program(pause_when_error=true)`（GDScript / Python / Lua / Rust） |
| 返回 | 除 C++ 外全部返回 `ParseResult {err, val}`；`val` 是出错前已解析出的语句（**两种模式下都保留**） |
| `pauseWhenError` | `true`（默认）：遇到第一个错误立即停止；`false`：尽力恢复、收集全部错误。八套语义一致 |
| 错误容器 | 公开字段 `errors`，格式 `line N: msg`；转义失败与 unexpected token 都记在这里 |
| 内部游标 | C++ 公开字段 `curToken`；其余七套私有（GDScript / Node.js 以 `_token` / `_failed` 下划线约定私有） |

## 5. 转义展开

引号串的转义（`\n` `\t` `\\` `\'` `\"` `\v` `\b` `\f`，没有续行）在**语法层**展开，
失败时保留失败点之前已展开的部分。除内建路径外，多数实现还提供独立函数：

| 实现 | 形态 | 返回 |
| --- | --- | --- |
| C++ | 无独立函数（`Parser::escape` 私有） | — |
| GDScript | 注入点 `lexer.escape_func`，默认 `Nevext.defalt_escape(s)`（静态方法） | `Result`（`err` + 展开后 `val`） |
| Node.js | 导出函数 `expandEscapes(src)` | `{err, val, reason}` |
| C# | 静态方法 `Lexer.ExpandEscapes(src)` | 元组 `(Result Err, string Val, string Reason)` |
| Python | 模块函数 `expand_escapes(src)` | `(err, val, reason)` |
| Lua | `Nevext.expand_escapes(src)` | 三返回值 `(err, val, reason)` |
| Go | 无独立函数（内联在 Parser） | — |
| Rust | `expand_escapes(src)` | `(Status, String, &'static str)` |

`reason` 取 `"unclosed"` / `"invalid"` / `""`，供错误消息拼装；`val` 失败时是**部分展开**的结果。

## 6. 字符串模型与元素计数

| 模型 | 实现 | 元素计数 |
| --- | --- | --- |
| UTF-16 码元 | C++（`u16string`）· Node.js · C# | 代理对算 2（X2） |
| 单码元 / 码点 | GDScript · Python | 代理对算 1 |
| UTF-8 字节序列 | Lua · Go · Rust | 非 BMP 字符按字节算（LU1 / GO1 / RS1）；Rust 的 `&str` 要求合法 UTF-8，孤立代理项与非 UTF-8 字节无法表达（RS2） |

**内容八套逐字节一致**（通用语料 H02 锁定）；只有元素计数不同。已登记为既定差异
（[通用测试计划](test-plan.md) §X2 / 各私有计划），不修。

## 7. 差异登记

| # | 差异 | 状态 |
| --- | --- | --- |
| D-1 | C++ 的 `Result` 多 `UnknownError` / `UnclosedStringError` 两个内部值 | 语言使然（`scanQuotedStr` 需要区分失败原因），对外行为不变，**保留** |
| D-2 | Token 原文字段：C++ 叫 `value`（视图），其余叫 `literal`（拥有）；Rust 的类型字段叫 `kind` | 语言使然 / 命名取舍，**保留** |
| D-3 | C++ 的 `ParseProgram` 走出参，其余七套返回 `{err, val}` | 语言使然（参考实现的既有风格），**保留** |
| D-4 | Rust 的 `Parser` 不绑定词法器，`parse_program(&mut lexer, ...)` 显式传参 | 语言使然（规避自引用生命周期），**保留** |
| D-5 | GDScript 的 Token 占位文本：EOF 填 `'<EOF>'`、未闭合串填 `'< unclosed string >'`；其余七套为空串 | GDScript 无空值便利，**保留** |
| D-6 | 转义展开入口不同（§5）：GDScript 可注入、JS/C#/Py/Lua/Rust 有独立函数、C++/Go 只内建 | 语言使然，**保留** |
| D-7 | 错误消息常量的公开面不同（§1） | 语言使然，**保留** |
| D-8 | `exception: line N: undefined token type <数值>` 兜底分支只有六套有（GD / JS / C# / Py / Lua / Go），C++ 与 Rust 没有 | 防御代码且不可达（TokenType 五值全部被处理），dump 门禁碰不到；**保留**，新增实现自行取舍 |

> 历史：v0.2.0 及之前 GDScript 的解析方法叫 `parse_prog`，v0.3.0 起统一为
> `parse_program`（破坏性变更，见 CHANGELOG）。

## 8. 相关文件

- 语法与边界：[语法概述](overview.md)
- 行为契约：[跨实现通用测试计划](test-plan.md)（55 条语料、断言约定、dump / demo 双门禁）
- 各实现使用指南：[C++](guide-cpp.md) · [GDScript](guide-gdscript.md) · [Node.js](guide-nodejs.md) ·
  [C#](guide-csharp.md) · [Python](guide-python.md) · [Lua](guide-lua.md) ·
  [Go](guide-go.md) · [Rust](guide-rust.md)
- 各实现私有测试计划：`<impl>/docs/test-plan.md`（缺口 X2 / LU1 / GO1 / RS1 的详情在其中）
