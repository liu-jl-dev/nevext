# Nevext Go 使用指南

本指南介绍 Go 实现（Go 1.21+）的集成方式与 API 用法。关于语言本身的语法规则，
请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现逐条一致，并由 60 条跨实现
通用语料与 dump 门禁守护（见[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，库源码位于 module 根目录 `go/nevext.go`（包名 `nevext`），
**import 只有一层**——工程布局与 [stope-lang](https://github.com/liu-jl-dev/stope-lang)
的 Go 侧一致。拷进你的工程时放进任意包目录即可，`go.mod` 不需要跟着走
——它只为仓库内的测试入口存在：

```go
package main

import nevext "你的模块路径/nevext"

func main() {
	lexer := nevext.NewLexer("windowSize 1280, 720;")
	parser := nevext.NewParser(lexer)
	res := parser.ParseProgram(true)
	if res.Err == nevext.Error {
		for _, e := range parser.Errors {
			println(e)
		}
	}
	for _, stmt := range res.Val {
		println(stmt.Line, strings.Join(stmt.Strings, " "))
	}
	// line 1: windowSize 1280 720
}
```

要点：

- **Go 1.21+**，零第三方依赖，无需安装；
- Go 的 string 是 **UTF-8 字节序列**：本语法的全部结构字符都在 ASCII 区间，
  UTF-8 多字节字符不可能包含它们，因此按字节扫描与按码元扫描行为完全等价，
  中文与 emoji 原样保留，无需转码；
- 命名按 Go 惯例 **PascalCase 导出**（`NextToken` / `ParseProgram` / `Reset`），
  错误容器是公开字段（`lexer.Errors` / `parser.Errors`）；
- `Result` 的 `Success == 0`，可直接写 `if res.Err != nevext.Success`。

## API 参考

版本常量 `nevext.Version` 与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```go
nevext.TokenType   // Unexpected=0, EndOfFile=1, RawStr=2, QuotedStr=3, Sep=4
nevext.Result      // Success=0, Error=1

Token        // {Type, Line, Literal}；Literal 为码元原文
Statement    // {Line, Strings}；Strings 为展开转义后的字符串数组
ParseResult  // {Err, Val}；Val 为 Statement 数组
```

`res.Val` 在出错时仍保留出错前已解析出的语句（对照 C++ 的出参 `prog`）。

### Lexer

```go
nevext.NewLexer(code)   // 构造即解析
lexer.Reset(code)       // 行号、游标、错误容器全部回到初始状态
lexer.NextToken()       // 输入耗尽后重复调用一直得到 EndOfFile

lexer.Line              // 当前行号
lexer.Errors            // 字符串数组，如 "line 1: unclosed string literal. "
```

错误消息与 C++ 逐字一致（含句号后的空格），常量为
`nevext.UnclosedMessage` / `InvalidEscapeMessage` / `UnexpectedMessage`。

### Parser

```go
nevext.NewParser(lexer)
parser.Reset(lexer)                     // 重绑词法器并清空 Errors
parser.ParseProgram(pauseWhenError)     // -> ParseResult；默认传 true

parser.Errors                           // 语法 / 转义错误
nevext.ExpandEscapes(src)               // -> (err, val, reason)
```

- `pauseWhenError = true`（默认）遇到第一个错误立即停止；`false` 时跳过错误
  继续解析，适合做错误恢复或 linter；
- 语句列表走**返回值** `res.Val`，不用 C++ 的出参风格；
- `ExpandEscapes` 是语法层转义展开的包级函数，失败时 `val` 保留失败点之前
  已展开的部分，`reason` 为 `"unclosed"` 或 `"invalid"`。

### 错误处理

| 容器 | 阶段 | 消息示例 |
| --- | --- | --- |
| `lexer.Errors` | 词法阶段 | `line 1: unclosed string literal.` |
| `parser.Errors` | 语法 / 转义阶段 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

转义只在语法层对引号串展开，失败时保留失败点之前已展开的部分；
同一行的多处错误各记一条，不做去重。复用 `Parser` 前请调用 `parser.Reset(...)`，
只有 `Reset` 会清空错误容器。

## 转义函数（可替换）

v0.4.0 起默认转义为 **NevextEscape**（原九项表 + `\uXXXX`，对 v0.3.0 及
之前向后兼容；`\u` 表项与 [json.org](https://www.json.org/json-zh.html)
语法图 / ECMA-404 一致，见[语法概述](./overview.md#转义字符)）。
`parser.EscapeFunc`（类型 `EscapeFunc`）可替换（构造可变参数亦可）：

```go
parser := nevext.NewParser(lexer)                      // 默认 NevextEscape
parser := nevext.NewParser(lexer, nevext.NullEscape)   // 不转义，原样返回
parser := nevext.NewParser(lexer, nevext.JsonEscape)   // 严格 JSON 表（无 \' 与 \v，多 \/）
```

三种函数签名统一为 `func(string) (Result, string, string)`；失败时 val
保留已展开前缀。历史入口 `ExpandEscapes(src)` 保留（= `NevextEscape` 的同义词）。

## 与其它实现的差异

| 项 | C++ | JavaScript | C# | Python | Lua | Go |
| --- | --- | --- | --- | --- | --- | --- |
| 方法名 | `NextToken` / `ParseProgram` | `nextToken` / `parseProgram` | `NextToken` / `ParseProgram` | `next_token` / `parse_program` | `next_token` / `parse_program` | `NextToken` / `ParseProgram` |
| 语句列表 | 出参 `Program& prog` | 返回值 `res.val` | 返回值 `res.Val` | 返回值 `res.val` | 返回值 `res.val` | 返回值 `res.Val` |
| 对象复用 | `Reset(...)` | `reset(...)` | `Reset(...)` | `reset(...)` | `reset(...)` | `Reset(...)` |
| 字符串模型 | `u16string`（UTF-16） | `string`（UTF-16） | `string`（UTF-16） | `str`（码点序列） | `string`（UTF-8 字节序列） | `string`（UTF-8 字节序列） |
| 扫描方式 | `string_view` 前移 | 游标 `_pos` | 游标 `_pos` | 游标 `pos` | 游标 `pos` | 游标 `Pos` |

除命名与返回值形态外，各实现行为契约完全一致。注意元素模型的差别：非 BMP 字符在
Go 里按 UTF-8 占多个字节，在 Python / GDScript 里算 1 个元素，在 UTF-16 系实现里
算 2 个码元——这只影响元素计数（`len`），不影响内容。

## 运行示例

[`go/demo/demo01.go`](../../go/demo/demo01.go) 是可直接运行的完整示例，
与其他 demo 使用同一段输入与同一输出格式：

```bash
go run ./demo                # 内置示例（在 go/ 目录内）
go\demo.cmd [文件]           # Windows 便捷入口（任意 cwd）
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.Errors` 与 `parser.Errors`
两个容器）；`2` 文件打不开。各份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- 字符串按**字节**计数（缺口 GO1）：`len(stmt.Strings[i])` 对非 ASCII 内容是
  字节数，需要字符数时请在调用方用 `unicode/utf8`（`utf8.RuneCountInString`）
  换算；
- Go string 可容纳任意字节（孤立高位字节、非 UTF-8 序列都会原样保留），
  这是与 Rust / Python 的差异点，行为见通用语料的字节保真用例；
- 性能与 C++ 相比有常数级差距（游标实现，整体 O(n)），大输入请自行评估。

> AI生成
