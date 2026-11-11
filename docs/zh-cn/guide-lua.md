# Nevext Lua 使用指南

本指南介绍 Lua 实现（Lua 5.4+）的集成方式与 API 用法。关于语言本身的语法规则，
请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现逐条一致，并由 60 条跨实现
通用语料与 dump 门禁守护（见[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，把 `lua/nevext/nevext.lua` 拷进工程即可（放哪个目录都行）：

```lua
local Nevext = require("nevext")   -- 按你的放置方式调整 package.path

local lexer = Nevext.Lexer.new("windowSize 1280, 720;")
local parser = Nevext.Parser.new(lexer)
local res = parser:parse_program()
if res.err == Nevext.Result.Error then
  print(table.concat(parser.errors, "\n"))
end
for _, stmt in ipairs(res.val) do
  print("line " .. stmt.line .. ": " .. table.concat(stmt.strings, " "))
end
-- line 1: windowSize 1280 720
```

要点：

- **Lua 5.4+**，零第三方依赖，无需安装（用到 5.3+ 的 `\u{XXXX}` 字符串转义与
  5.4 的 `os.exit(code, true)`）；
- Lua 的 string 是**字节序列**：本语法的全部结构字符都在 ASCII 区间，UTF-8
  多字节字符不可能包含它们，因此按字节扫描与按码元扫描行为完全等价，中文与
  emoji 原样保留，无需转码；
- 命名按 Lua 惯例 snake_case（`next_token` / `parse_program` / `reset`），
  方法用**冒号调用**（`lexer:next_token()`），构造器统一 `Xxx.new(...)`；
- `Result.Success == 0`，可直接写 `if res.err ~= 0`。

## API 参考

版本常量 `Nevext.VERSION` 与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```lua
Nevext.TokenType  -- Unexpected=0, EndOfFile=1, RawStr=2, QuotedStr=3, Sep=4
Nevext.Result     -- Success=0, Error=1

Token             -- {type, line, literal}；literal 为码元原文
Statement         -- {line, strings}；strings 为展开转义后的字符串数组
解析结果          -- {err, val}；val 为 Statement 数组
```

`res.val` 在出错时仍保留出错前已解析出的语句（对照 C++ 的出参 `prog`）。

### Lexer

```lua
Nevext.Lexer.new(code)   -- 构造即解析；code 的存活期须覆盖解析过程
lexer:reset(code)        -- 行号、游标、错误容器全部回到初始状态
lexer:next_token()       -- 输入耗尽后重复调用一直得到 EndOfFile

lexer.line               -- 当前行号
lexer.errors             -- 字符串数组，如 "line 1: unclosed string literal. "
```

错误消息与 C++ 逐字一致（含句号后的空格），常量为
`Nevext.UNCLOSED_MESSAGE` / `INVALID_ESCAPE_MESSAGE` / `UNEXPECTED_MESSAGE`。

### Parser

```lua
Nevext.Parser.new(lexer)
parser:reset(lexer)                             -- 重绑词法器并清空 errors
parser:parse_program(pause_when_error)          -- -> {err, val}；默认 true

parser.errors                                   -- 语法 / 转义错误
Nevext.expand_escapes(src)                      -- -> (err, val, reason)
```

- `pause_when_error = true`（默认）遇到第一个错误立即停止；`false` 时跳过错误
  继续解析，适合做错误恢复或 linter；
- 语句列表走**返回值** `res.val`，不用 C++ 的出参风格；
- `expand_escapes` 是语法层转义展开的模块级函数，失败时 `val` 保留失败点之前
  已展开的部分，`reason` 为 `"unclosed"` 或 `"invalid"`。

### 错误处理

| 容器 | 阶段 | 消息示例 |
| --- | --- | --- |
| `lexer.errors` | 词法阶段 | `line 1: unclosed string literal.` |
| `parser.errors` | 语法 / 转义阶段 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

转义只在语法层对引号串展开，失败时保留失败点之前已展开的部分；
同一行的多处错误各记一条，不做去重。复用 `Parser` 前请调用 `parser:reset(...)`，
只有 `reset` 会清空错误容器。

## 转义函数（可替换）

v0.4.0 起默认转义为 **nevext_escape**（原九项表 + `\uXXXX`，对 v0.3.0 及
之前向后兼容；`\u` 表项与 [json.org](https://www.json.org/json-zh.html)
语法图 / ECMA-404 一致，见[语法概述](./overview.md#转义字符)）。
`parser.escape_func` 或构造第二参数注入：

```lua
local parser = Nevext.Parser.new(lexer)                          -- 默认 nevext_escape
local parser = Nevext.Parser.new(lexer, Nevext.null_escape)      -- 不转义，原样返回
local parser = Nevext.Parser.new(lexer, Nevext.json_escape)      -- 严格 JSON 表（无 \' 与 \v，多 \/）
```

三种函数签名统一为 `src -> (err, val, reason)`；失败时 `val` 保留已展开
前缀。历史入口 `Nevext.expand_escapes(src)` 保留（= `nevext_escape` 的同义词）。

## 与其它实现的差异

| 项 | C++ | JavaScript | C# | Python | Lua |
| --- | --- | --- | --- | --- | --- |
| 方法名 | `NextToken` / `ParseProgram` | `nextToken` / `parseProgram` | `NextToken` / `ParseProgram` | `next_token` / `parse_program` | `next_token` / `parse_program` |
| 语句列表 | 出参 `Program& prog` | 返回值 `res.val` | 返回值 `res.Val` | 返回值 `res.val` | 返回值 `res.val` |
| 对象复用 | `Reset(...)` | `reset(...)` | `Reset(...)` | `reset(...)` | `reset(...)` |
| 字符串模型 | `u16string`（UTF-16） | `string`（UTF-16） | `string`（UTF-16） | `str`（码点序列） | `string`（UTF-8 字节序列） |
| 扫描方式 | `string_view` 前移 | 游标 `_pos` | 游标 `_pos` | 游标 `pos` | 游标 `pos` |

除命名与返回值形态外，各实现行为契约完全一致。注意元素模型的差别：非 BMP 字符在
Lua 里按 UTF-8 占多个字节，在 Python / GDScript 里算 1 个元素，在 UTF-16 系实现里
算 2 个码元——这只影响元素计数（`#` / `len`），不影响内容。

## 运行示例

[`lua/demo/demo01.lua`](../../lua/demo/demo01.lua) 是可直接运行的完整示例，
与另外五份 demo 使用同一段输入与同一输出格式：

```bash
lua lua/demo/demo01.lua                # 内置示例
lua lua/demo/demo01.lua 某个文件.txt    # 解析该文件（UTF-8）
lua\demo.cmd [文件]                    # Windows 便捷入口
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.errors` 与 `parser.errors`
两个容器）；`2` 文件打不开。六份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- **面向 Lua 5.4+**：实现用到 5.3+ 的 `\u{XXXX}` 转义与 5.4 的 `os.exit` 双参形态，
  LuaJIT / 5.1–5.3 理论上大多可用，但未经本仓库测试守护；
- 字符串按**字节**计数（缺口 LU1）：`#stmt.strings[i]` 对非 ASCII 内容是字节数，
  需要字符数时请在调用方用 `utf8.len`（Lua 5.3+ 标准库）换算；
- 性能介于 C++ 与 GDScript 之间（游标实现，整体 O(n)），大输入请自行评估。

> AI生成
