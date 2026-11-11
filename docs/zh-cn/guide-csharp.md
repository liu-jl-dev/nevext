# Nevext C# 使用指南

本指南介绍 C# 实现（.NET）的集成方式与 API 用法。关于语言本身的语法规则，
请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现逐条一致，并由 60 条跨实现
通用语料与 dump 门禁守护（见[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，把 `csharp/nevext/Nevext.cs` 拷进工程即可（`namespace Nevext`）：

```csharp
using Nevext;

var lexer = new Lexer("windowSize 1280, 720;");
var parser = new Parser(lexer);
var res = parser.ParseProgram();
if (res.Err == Result.Error)
{
    Console.Error.WriteLine(string.Join("\n", parser.Errors));
}
foreach (var stmt in res.Val)
{
    Console.WriteLine($"line {stmt.Line}: [{string.Join(", ", stmt.Strings)}]");
}
// line 1: [windowSize, 1280, 720]
```

要点：

- **.NET 任意近期版本**（本仓库用 .NET SDK 10 验证）；只用最基础的 BCL 类型
  （`List<T>`、`Dictionary<K,V>`、`StringBuilder`），Unity / Mono 理论上可用但未验证；
- 零第三方依赖，无 NuGet 引用；
- C# 字符串就是 UTF-16 码元序列，中文、emoji 直接可用，无需转码；
- 命名与 C++ 参考实现同为 PascalCase，迁移 C++ 调用代码时基本只改大小写；
- `Result.Success == 0`，可直接写 `res.Err == Result.Error`。

## API 参考

版本常量 `Nevext.Info.Version`（`public const string`）与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```csharp
TokenType  // { Unexpected = 0, EndOfFile = 1, RawStr = 2, QuotedStr = 3, Sep = 4 }
Result     // { Success = 0, Error = 1 }

Token      // { Type: TokenType, Line: int, Literal: string }
Statement  // { Line: int, Strings: List<string> }
ParseResult// { Err: Result, Val: List<Statement> }
```

`ParseResult.Val` 在出错时仍保留出错前已解析出的语句（对照 C++ 的出参 `prog`）。

### Lexer

```csharp
public Lexer(string input = "");
public void Reset(string input = "");  // 行号、游标、错误容器全部回到初始状态
public Token NextToken();              // 输入耗尽后重复调用一直得到 EndOfFile

public int Line { get; }               // 当前行号
public List<string> Errors { get; }    // 词法错误，如 "line 1: unclosed string literal. "
```

错误消息与 C++ 逐字一致（含句号后的空格），常量收敛在
`Lexer.UnclosedMessage` / `Lexer.InvalidEscapeMessage` / `Lexer.UnexpectedMessage`。

### Parser

```csharp
public Parser(Lexer lexer);
public void Reset(Lexer lexer);                       // 重绑词法器并清空 Errors
public ParseResult ParseProgram(bool pauseWhenError = true);

public List<string> Errors { get; }                   // 语法 / 转义错误
public static (Result Err, string Val, string Reason) ExpandEscapes(string src);
```

- `pauseWhenError = true`（默认）遇到第一个错误立即停止；`false` 时跳过错误
  继续解析，适合做错误恢复或 linter；
- 语句列表走**返回值** `ParseResult.Val`，不用 C++ 的出参风格；
- `ExpandEscapes` 是语法层转义展开的公开静态方法，失败时 `Val` 保留失败点之前
  已展开的部分，`Reason` 为 `"unclosed"` 或 `"invalid"`。

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
`parser.EscapeFunc`（`EscapeFunc` 委托）可替换：

```csharp
var parser = new Parser(lexer);
parser.EscapeFunc = Parser.NullEscape;     // 不转义，原样返回
parser.EscapeFunc = Parser.NevextEscape;   // 默认：九项 + \uXXXX
parser.EscapeFunc = Parser.JsonEscape;     // 严格 JSON 表（无 \' 与 \v，多 \/）
```

三种函数签名统一为 `string -> (Result Err, string Val, string Reason)`；
失败时 `Val` 保留已展开前缀。历史入口 `Parser.ExpandEscapes(src)` 保留
（= `NevextEscape` 的同义词）。

## 与 C++ / JavaScript 实现的差异

| 项 | C++ | JavaScript | C# |
| --- | --- | --- | --- |
| 方法名 | `NextToken` / `ParseProgram` | `nextToken` / `parseProgram` | `NextToken` / `ParseProgram` |
| 语句列表 | 出参 `Program& prog` | 返回值 `res.val` | 返回值 `res.Val` |
| 对象复用 | `Reset(...)` | `reset(...)` | `Reset(...)` |
| 字符串模型 | `u16string`（UTF-16） | `string`（UTF-16） | `string`（UTF-16） |
| 扫描方式 | `string_view` 前移 | 游标 `_pos` | 游标 `_pos` |

除命名与返回值形态外，三者行为契约完全一致。

## 运行示例

[`csharp/demo/Demo01.cs`](../../csharp/demo/Demo01.cs) 是可直接运行的完整示例（独立工程
`demo/Demo01.csproj`，只编译自身与 `nevext/Nevext.cs`），与另外四份 demo 使用同一段输入
与同一输出格式：

```bash
dotnet run --project csharp/demo                   # 内置示例
dotnet run --project csharp/demo -- 某个文件.txt    # 解析该文件（UTF-8）
csharp\demo.cmd [文件]                             # Windows 便捷入口
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `Lexer.Errors` 与 `Parser.Errors`
两个容器）；`2` 文件打不开。五份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- **.NET 系运行时**：实现本体只用基础 BCL API，Unity / Mono 理论上可直接内嵌，
  但未经本仓库测试守护；
- 性能介于 C++ 与 GDScript 之间（游标实现，整体 O(n)），大输入请自行评估。

> AI生成
