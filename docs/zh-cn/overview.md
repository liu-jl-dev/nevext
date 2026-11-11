# Nevext 数据描述语言
Nevext 是一个汇编语言风格的数据描述语言（DDL）。每一个语句解析为一个字符串列表。整个程序为语句的列表。

Nevext 仅规定如何解析代码字符串，不规定语义。

“Nevext”意为“never extends”，永远不增加新的语法。

容易手写实现 Nevext 解析器是 Nevext 的目标之一。可以预见，容易手写实现也就容易使用 LLM 生成实现。手写实现与 LLM 生成实现，能够方便平台移植。

## 开源协议
本项目使用 MIT 协议。

## 实现
本仓库中已有的 Nevext 解析器实现：
- C++
- GDScript（实验）

## Nevext 语法
程序与语句的语法
```
prog : stmt*

stmt : str* ";"

str : sq_str | dq_str | raw_str
```
其中 raw_str 是连续的非空白字符以及“,”、“;”的字符串。

sq_str 是以单引号“'”开头、结尾的字符串。若字符串中需要包含单引号，可以使用转义字符“\\'”。

dq_str 是以双引号“\"”开头、结尾的字符串。同样可以转义“\\\"”。

Nevext 支持注释。单行注释以“//”开头。多行注释以“/\*”开头，以“\*/”结尾。

未闭合的多行注释**不是错误**：从“/\*”起直到输入结束的所有内容都被当作注释。

**注意：** Nevext 解析器将逗号“,” 视为空白字符，与空格“ ”的效果相同。可以随意用逗号分隔相邻的字符串，以增加可读性。例如
```
windowSize 1280, 720;
```
将被解析为字符串列表
```
["windowSize", "1280", "720"]
```
但使用连续的逗号时
```
windowSize 1280, , 720 ,;
```
同样会被解析为
```
["windowSize", "1280", "720"]
```

**细节**
- 有引号的字符串中可以换行，解析结果将包含换行符。字符串内部的换行同样计入行号。
- 没有字符串，只有一个分号“;”在语法上是一个空语句，解析结果不会保留空语句。
- prog 的最后一个语句的结束处可以没有分号“;”。

## 转义字符
raw_str 不进行转义。

sq_str 与 dq_str 支持的转义字符由**转义函数**决定。八套实现都内置三种转义
函数（v0.4.0 起默认 `nevext_escape`）：

- `null_escape`：不转义，转义序列原样保留（永不报错）；
- `nevext_escape`：**默认**，原九项表 + `\uXXXX`；
- `json_escape`：严格 JSON 表（互操作校验用）。

### Nevext 默认转义（`nevext_escape`）

v0.1 起的九项表，加上 v0.4.0 新增的 `\uXXXX`：

| 转义字符 | 转义结果 | 码点 |
| --- | --- | --- |
| \\' | ' | U+0027 |
| \\" | " | U+0022 |
| \\\\ | \\ | U+005C |
| \\n | 换行符 | U+000A |
| \\r | 回车符 | U+000D |
| \\t | 制表符 | U+0009 |
| \\v | 垂直制表符 | U+000B |
| \\b | 退格符 | U+0008 |
| \\f | 换页符 | U+000C |
| \\uXXXX | 十六进制码点 U+XXXX 对应的字符 | 任意（4 位 hex，大小写均可） |

要点：

- **对 v0.3.0 及之前向后兼容**：既有转义序列行为不变，只有 `\u` 从非法
  变合法。
- **转义表与引号种类无关**：`\'` 与 `\"` 在单引号串、双引号串里都合法
  （与 C / Python / JS / Lua 的双引号语言惯例一致）。
- `\uXXXX` 的表项与 JSON 官网 [json.org](https://www.json.org/json-zh.html)
  字符串语法图（McKeeman 产生式，标准 ECMA-404）的 `u` 表项一致；`\/`
  不在默认表（见下）。
- 不在表内的转义序列（如 `\/`、`\q`）是**非法转义**：报
  `invalid escape character.` 错误，语句保留已展开的前缀（见各实现的错误
  恢复语义）。`\u` 后不足 4 位十六进制同样非法。

> `\uXXXX` 的跨实现差异：产出各实现内部表示的对应字符（C++ / JS / C# 为
> UTF-16 码元，Python / GDScript 为码点，Go / Lua / Rust 按 UTF-8 编码）。
> **代理区码点（D800–DFFF）**在 C++ / JS / C# / Python 落成代理单元，
> Go 落成 U+FFFD，Rust / Lua 报非法转义——跨实现对拍请只用非代理码点。

### 严格 JSON 转义（`json_escape`，可选）

注入 `json_escape` 后切换为 JSON 的 escape 产生式（共 9 种）：

```
escape: '"' '\' '/' 'b' 'f' 'n' 'r' 't' 'u' hex hex hex hex
```

与默认表的差异：**没有 `\'` 与 `\v`，多出 `\/`**（展开为 `/`）。适合
「输入必须只含 JSON 合法转义」的互操作校验场景。

### 转义函数的注入

三种函数都可在解析前注入（详见各实现的使用指南与
[API Reference](./api-reference.md)）：C++ `Parser::escapeFunc`、
Python `Parser(lexer, escape_func=...)`、Node.js `parser.escapeFunc`、
C# `parser.EscapeFunc`、Go `parser.EscapeFunc`、Rust `parser.escape_func`、
Lua `parser.escape_func`、GDScript `lexer.escape_func`。

> AI生成
