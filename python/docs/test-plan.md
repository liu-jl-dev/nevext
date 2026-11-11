# Nevext Python 实现测试计划

> 适用范围：`python/nevext/nevext.py`（Python 3.10+，单文件，零第三方依赖）
> 测试代码目录：`python/test/`
> **跨实现通用语料不在本文档**：六套实现共用的 60 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `python/test/test_conformance.py`。本文档只描述 Python 私有的 `Impl` 套件。
> 文档最后更新：2026-11-10（v0.4.0：默认转义 nevext_escape；65 用例 / 481 断言通过）

## 1. 目标与非目标

**目标**

1. 用与 C++ / GDScript / JavaScript / C# 相同的口径守住 Python 实现的行为：
   `Conformance.*` 60 条语料 + dump 门禁（`test_main.py --dump` 与 C++ 侧输出必须
   逐字节一致）。
2. 覆盖 Python 侧的工程性关注点：对象复用（reset）、终止性、大输入、码点保真。
3. 保持零依赖：不用 unittest / pytest，`python test/test_main.py` 一条命令即跑。

**非目标**

- 不测 PyPy / MicroPython 等替代运行时（实现只用标准库最基础的语法与容器，
  理论上可用，但不在本计划的验证范围内）。
- 不做性能基准：Python 与 C++ 的性能差异属实现差异（见 `docs/zh-cn/test-plan.md` X3 的同款口径）。

## 2. 被测对象与 API 形状

单文件 `nevext/nevext.py`（模块名 `nevext`，拷入工程即用），公开成员：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | `IntEnum`，5 项，取值顺序与 C++ 一致（`Unexpected`=0 … `Sep`=4） |
| `Result` | `IntEnum`，`Success`=0 / `Error`=1 |
| `Lexer` | `Lexer(code="")`、`next_token()`、`reset(code="")`；`line`、`errors: list[str]` |
| `Parser` | `Parser(lexer)`、`parse_program(pause_when_error=True)`、`reset(lexer)`；`errors: list[str]` |
| `Statement` / `ParseResult` / `Token` | 产物类型（`__slots__` 轻量类） |
| `expand_escapes` | 转义展开（模块级函数，供测试与工具使用） |

与 C++ 的差异：`parse_program` 走**返回值**（`ParseResult { err, val }`），不用 C++ 的出参；
命名按 Python 惯例 snake_case（`next_token` / `parse_program`）。除此之外的行为契约
与 C++ 逐条一致。

## 3. 实现要点（Python 特有）

1. **码点模型。** Python 的 `str` 是 Unicode 码点序列（近似 UTF-32）：非 BMP 字符算 1 个元素，
   而 C++ / JS / C# 内部是 2 个 UTF-16 码元。通用语料只断言内容相等，所以这一差异
   不影响契约（文档 X2），但写语料时要注意：**H02 的 emoji 在 Python 里写成一个码点
   `\U0001F600`**，而不是代理对，否则 `print` 会因不可编码而失败。
   孤立代理项 Python 允许存在于 `str` 中，`Impl.LoneSurrogatePreserved` 按码点原样保留。
2. **游标代替切片。** 与 C++ 的 `string_view` 前移、GDScript 的逐字符 `substr()` 不同，
   Python 用「完整字符串 + `pos` 游标 + 一次性切片」，行为一致且整体 O(n)。
   `reset` 同时复位游标。
3. **stdout 编码。** dump 含中文与非 BMP 字符（H02 / H04），`test_main.py` 对 stdout 做
   `reconfigure(encoding="utf-8", errors="backslashreplace", newline="\n")`：
   - `encoding="utf-8"`：Windows 控制台默认代码页不是 UTF-8；
   - `errors="backslashreplace"`：孤立代理项之类无法编码的字符降级成 `\udXXX` 文本，
     保证诊断信息不会因编码异常打断整轮测试；
   - `newline="\n"`：避免文本模式把 `\n` 翻成 `\r\n`，让 dump 直接可比。
4. **错误消息逐字对齐 C++**（含句号后的空格，消息收敛为 `UNCLOSED_MESSAGE` 等三个常量），
   一致性断言通过「消息 → (行号, 种类)」映射比较，与另四套相同。
5. **dump 的 `show()` 必须与 C++ 的 `EscapeVisible` 逐项一致**：六个具名控制字符
   （`\n` `\r` `\t` `\v` `\b` `\f`）、反斜杠翻倍、其余控制字符 `\xNN`（大写两位十六进制）；
   引号在 `show` 之后再转义（「先反斜杠、后引号」）。JS 侧第一版就漏过这两类，
   靠 dump 门禁抓出——**改动 dump 相关代码后必须重跑门禁**。

## 4. 运行方式

```bash
# 全部用例（Conformance 60 条 + Impl 5 条）
python python/test/test_main.py      # 或 python\test.cmd

# 按名字子串筛选
python python/test/test_main.py Conformance.C0

# dump 门禁（与 C++ 逐字节 diff）
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(python python/test/test_main.py --dump)
```

**当前基线（CPython 3.13 / 3.14，Windows）**：65 个用例 / 481 条断言全部通过
（Conformance 55 + Impl 5）。两个版本都实测过。

## 5. 用例清单：`Impl` 套件（5 个，test_impl.py）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| PY-01 | `ResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.line` 回到 1、两侧错误容器为空（对应 GDScript IMP-02、C++ LEX-26 / PAR-18、JS JS-01、CS-01） |
| PY-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile`（不死循环、不爆栈） |
| PY-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字符 |
| PY-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| PY-05 | `LoneSurrogatePreserved` | `x <孤立高代理> y "<孤立低代理>"` | 孤立代理项按码点原样保留，无错误（与 JS / C# 同语义） |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与其它实现相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |
| **PY1** | `show()` 输出的孤立代理项无法用 UTF-8 编码，靠 stdout 的 `backslashreplace` 兜底渲染成 `\udXXX` 文本 | 只影响诊断可读性，不影响断言与 dump（60 条语料不含孤立代理项） | 接受现状；若将来要把代理项写进 dump，需在 `show()` 里显式转义 `\uD800`–`\uDFFF` 并同步 C++ 侧 |

## 7. 目录结构

```
python/
├── nevext/
│   └── nevext.py           # 库本体：单文件、零依赖，拷入任意工程即用
├── test/
│   ├── test_framework.py   # 零依赖自研框架（对齐 cpp/test 的设计取舍）
│   ├── test_conformance.py # 60 条通用语料 + dump
│   ├── test_impl.py        # Python 私有的 Impl 套件（5 条）
│   └── test_main.py        # 入口：--dump / 筛选 / 退出码 / stdout 编码
├── test.cmd                # Windows 便捷入口（纯 ASCII 注释）
├── demo.cmd                # Windows 便捷入口：跑示例
├── demo/
│   └── demo01.py           # 可运行示例
└── docs/test-plan.md       # 本文档
```

> AI生成
