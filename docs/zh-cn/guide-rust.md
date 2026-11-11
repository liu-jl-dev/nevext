# Nevext Rust 使用指南

本指南介绍 Rust 实现（Rust 1.70+，edition 2021）的集成方式与 API 用法。关于
语言本身的语法规则，请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现
逐条一致，并由 60 条跨实现通用语料与 dump 门禁守护（见
[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，把 `rust/nevext/nevext.rs` 拷进你的工程：

```toml
# Cargo.toml
[lib]
path = "nevext/nevext.rs"   # 或复制为常规的 src/lib.rs
```

```rust
use nevext::{Lexer, Parser, Status};

fn main() {
    let mut lexer = Lexer::new("windowSize 1280, 720;");
    let mut parser = Parser::new();
    let res = parser.parse_program(&mut lexer, true);
    if res.err == Status::Error {
        for e in &parser.errors {
            println!("{}", e);
        }
    }
    for stmt in &res.val {
        println!("line {}: {}", stmt.line, stmt.strings.join(" "));
    }
    // line 1: windowSize 1280 720
}
```

要点：

- **Rust 1.70+ / edition 2021**，零第三方依赖，无需安装；
- Rust 的 `&str` 是 **UTF-8 字节序列**：本语法的全部结构字符都在 ASCII 区间，
  UTF-8 多字节字符不可能包含它们，因此按字节扫描与按码元扫描行为完全等价，
  中文与 emoji 原样保留，无需转码；
- 结果枚举叫 **`Status`** 而不是 `Result`——后者会与 `std::result::Result`
  混淆；错误容器是公开字段（`lexer.errors` / `parser.errors`）；
- `Parser` **不绑定词法器**（借用检查器友好的惯用做法）：
  `parse_program(&mut lexer, pause_when_error)` 每次调用显式传入。

## API 参考

版本常量 `nevext::VERSION` 与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```rust
nevext::TokenType   // Unexpected=0, EndOfFile=1, RawStr=2, QuotedStr=3, Sep=4
nevext::Status      // Success=0, Error=1

Token        // {kind, line, literal}；literal 为码元原文
Statement    // {line, strings}；strings 为展开转义后的字符串数组
ParseResult  // {err, val}；val 为 Statement 数组
```

`res.val` 在出错时仍保留出错前已解析出的语句（对照 C++ 的出参 `prog`）。

### Lexer

```rust
Lexer::new(code)     // 构造即解析
lexer.reset(code)    // 行号、游标、错误容器全部回到初始状态
lexer.next_token()   // 输入耗尽后重复调用一直得到 EndOfFile

lexer.line           // 当前行号
lexer.errors         // Vec<String>，如 "line 1: unclosed string literal. "
```

错误消息与 C++ 逐字一致（含句号后的空格），常量为
`nevext::UNCLOSED_MESSAGE` / `INVALID_ESCAPE_MESSAGE` / `UNEXPECTED_MESSAGE`。

### Parser

```rust
Parser::new()
parser.reset()                                       // 清空 errors
parser.parse_program(lexer, pause_when_error)        // -> ParseResult；默认传 true

parser.errors                                        // 语法 / 转义错误
nevext::expand_escapes(src)                          // -> (err, val, reason)
```

- `pause_when_error = true`（默认）遇到第一个错误立即停止；`false` 时跳过错误
  继续解析，适合做错误恢复或 linter；
- 语句列表走**返回值** `res.val`，不用 C++ 的出参风格；
- `expand_escapes` 是语法层转义展开的 crate 级函数，失败时 `val` 保留失败点
  之前已展开的部分，`reason` 为 `"unclosed"` 或 `"invalid"`。

### 错误处理

| 容器 | 阶段 | 消息示例 |
| --- | --- | --- |
| `lexer.errors` | 词法阶段 | `line 1: unclosed string literal.` |
| `parser.errors` | 语法 / 转义阶段 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

转义只在语法层对引号串展开，失败时保留失败点之前已展开的部分；
同一行的多处错误各记一条，不做去重。复用 `Parser` 前请调用 `parser.reset()`，
只有 `reset` 会清空错误容器。

## 转义函数（可替换）

v0.4.0 起默认转义为 **nevext_escape**（原九项表 + `\uXXXX`，对 v0.3.0 及
之前向后兼容；`\u` 表项与 [json.org](https://www.json.org/json-zh.html)
语法图 / ECMA-404 一致，见[语法概述](./overview.md#转义字符)）。
`parser.escape_func`（类型 `EscapeFunc`）或 `Parser::with_escape` 注入：

```rust
use nevext::{Parser, null_escape, nevext_escape, json_escape};

let mut parser = Parser::new();                       // 默认 nevext_escape
let mut parser = Parser::with_escape(null_escape);    // 不转义，原样返回
let mut parser = Parser::with_escape(json_escape);    // 严格 JSON 表（无 \' 与 \v，多 \/）
```

三种函数签名统一为 `fn(&str) -> (Status, String, &'static str)`；失败时
val 保留已展开前缀。历史入口 `expand_escapes(src)` 保留（= `nevext_escape`
的同义词）。

## 与其它实现的差异

| 项 | C++ | JavaScript | C# | Python | Lua | Rust |
| --- | --- | --- | --- | --- | --- | --- |
| 方法名 | `NextToken` / `ParseProgram` | `nextToken` / `parseProgram` | `NextToken` / `ParseProgram` | `next_token` / `parse_program` | `next_token` / `parse_program` | `next_token` / `parse_program` |
| 语句列表 | 出参 `Program& prog` | 返回值 `res.val` | 返回值 `res.Val` | 返回值 `res.val` | 返回值 `res.val` | 返回值 `res.val` |
| 对象复用 | `Reset(...)` | `reset(...)` | `Reset(...)` | `reset(...)` | `reset(...)` | `reset()`（词法器不绑定） |
| 字符串模型 | `u16string`（UTF-16） | `string`（UTF-16） | `string`（UTF-16） | `str`（码点序列） | `string`（UTF-8 字节序列） | `&str`（UTF-8 字节序列） |
| 扫描方式 | `string_view` 前移 | 游标 `_pos` | 游标 `_pos` | 游标 `pos` | 游标 `pos` | 游标 `pos` |

除命名与返回值形态外，各实现行为契约完全一致。注意元素模型的差别：非 BMP 字符在
Rust 里按 UTF-8 占多个字节，在 Python / GDScript 里算 1 个元素，在 UTF-16 系实现里
算 2 个码元——这只影响元素计数（`len`），不影响内容。

## 运行示例

[`rust/demo/demo01.rs`](../../rust/demo/demo01.rs) 是可直接运行的完整示例，
与其他 demo 使用同一段输入与同一输出格式：

```bash
cargo run --bin demo01              # 内置示例（在 rust/ 目录内）
rust\demo.cmd [文件]                # Windows 便捷入口（任意 cwd）
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.errors` 与 `parser.errors`
两个容器）；`2` 文件打不开。各份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- 字符串按**字节**计数（缺口 RS1）：`stmt.strings[i].len()` 对非 ASCII 内容是
  字节数，需要字符数时请在调用方用 `str::chars().count()` 换算；
- `&str` 必须是**合法 UTF-8**（缺口 RS2）：孤立代理项与非 UTF-8 字节序列无法
  表达——Python 的 str 允许孤立代理项、Go 的 string 允许任意字节，Rust 做不到；
  输入保证为合法 UTF-8 时行为与其他实现一致；
- 性能与 C++ 相比有常数级差距（游标实现，整体 O(n)），大输入请自行评估。

> AI生成
