# Nevext Python 使用指南

本指南介绍 Python 实现（CPython 3.10+）的集成方式与 API 用法。关于语言本身的语法规则，
请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现逐条一致，并由 55 条跨实现
通用语料与 dump 门禁守护（见[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，把 `python/nevext/nevext.py` 拷进工程即可：

```python
import nevext

lexer = nevext.Lexer("windowSize 1280, 720;")
parser = nevext.Parser(lexer)
res = parser.parse_program()
if res.err == nevext.Result.Error:
    print("\n".join(parser.errors))
for stmt in res.val:
    print(f"line {stmt.line}: {stmt.strings}")
# line 1: ['windowSize', '1280', '720']
```

要点：

- **Python 3.10+**，只用标准库（`enum`），零第三方依赖，无需安装；
- Python 的 `str` 是 Unicode 码点序列，中文、emoji 直接可用，无需转码；
- 命名按 Python 惯例 snake_case（`next_token` / `parse_program` / `reset`），
  与 C++ 的 PascalCase 一一对应；
- `Result.Success == 0`，可直接写 `if res.err`。

## API 参考

版本常量 `nevext.__version__` 与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```python
TokenType  # IntEnum: Unexpected=0, EndOfFile=1, RawStr=2, QuotedStr=3, Sep=4
Result     # IntEnum: Success=0, Error=1

Token      # .type / .line / .literal
Statement  # .line / .strings: list[str]
ParseResult# .err / .val: list[Statement]
```

`ParseResult.val` 在出错时仍保留出错前已解析出的语句（对照 C++ 的出参 `prog`）。

### Lexer

```python
Lexer(code="")            # 构造即解析
lexer.reset(code="")      # 行号、游标、错误容器全部回到初始状态
lexer.next_token()        # 输入耗尽后重复调用一直得到 EndOfFile

lexer.line                # 当前行号
lexer.errors              # list[str]，如 "line 1: unclosed string literal. "
```

错误消息与 C++ 逐字一致（含句号后的空格），常量在模块级：
`nevext.UNCLOSED_MESSAGE` / `INVALID_ESCAPE_MESSAGE` / `UNEXPECTED_MESSAGE`。

### Parser

```python
Parser(lexer)
parser.reset(lexer)                              # 重绑词法器并清空 errors
parser.parse_program(pause_when_error=True)      # -> ParseResult

parser.errors                                    # list[str]，语法 / 转义错误
nevext.expand_escapes(src)                       # -> (err, val, reason)
```

- `pause_when_error=True`（默认）遇到第一个错误立即停止；`False` 时跳过错误
  继续解析，适合做错误恢复或 linter；
- 语句列表走**返回值** `ParseResult.val`，不用 C++ 的出参风格；
- `expand_escapes` 是语法层转义展开的模块级函数，失败时 `val` 保留失败点之前
  已展开的部分，`reason` 为 `"unclosed"` 或 `"invalid"`。

### 错误处理

| 容器 | 阶段 | 消息示例 |
| --- | --- | --- |
| `lexer.errors` | 词法阶段 | `line 1: unclosed string literal.` |
| `parser.errors` | 语法 / 转义阶段 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

转义只在语法层对引号串展开，失败时保留失败点之前已展开的部分；
同一行的多处错误各记一条，不做去重。复用 `Parser` 前请调用 `parser.reset(...)`，
只有 `reset` 会清空错误容器。

## 与其它实现的差异

| 项 | C++ | JavaScript | C# | Python |
| --- | --- | --- | --- | --- |
| 方法名 | `NextToken` / `ParseProgram` | `nextToken` / `parseProgram` | `NextToken` / `ParseProgram` | `next_token` / `parse_program` |
| 语句列表 | 出参 `Program& prog` | 返回值 `res.val` | 返回值 `res.Val` | 返回值 `res.val` |
| 对象复用 | `Reset(...)` | `reset(...)` | `Reset(...)` | `reset(...)` |
| 字符串模型 | `u16string`（UTF-16） | `string`（UTF-16） | `string`（UTF-16） | `str`（码点序列） |
| 扫描方式 | `string_view` 前移 | 游标 `_pos` | 游标 `_pos` | 游标 `pos` |

除命名与返回值形态外，各实现行为契约完全一致。注意码点模型的差别：非 BMP 字符在
Python 里算 1 个元素，在 UTF-16 系实现里算 2 个码元——这只影响 `len()`，不影响内容。

## 运行示例

[`python/demo/demo01.py`](../../python/demo/demo01.py) 是可直接运行的完整示例，
与 C++ / GDScript / JavaScript / C# / Python 五份 demo 使用同一段输入与同一输出格式：

```bash
python python/demo/demo01.py                # 内置示例
python python/demo/demo01.py 某个文件.txt    # 解析该文件（UTF-8）
python\demo.cmd [文件]                      # Windows 便捷入口
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.errors` 与 `parser.errors`
两个容器）；`2` 文件打不开。五份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- **面向 CPython**：实现只用标准库最基础的语法与容器，PyPy / MicroPython 理论上可用，
  但未经本仓库测试守护；
- 输出非 BMP 字符到终端时需保证 stdout 是 UTF-8（`test_main.py` 已经显式设置）；
  孤立代理项无法编码为 UTF-8，只能用 `errors="backslashreplace"` 之类的方式降级；
- 性能介于 C++ 与 GDScript 之间（游标实现，整体 O(n)），大输入请自行评估。
