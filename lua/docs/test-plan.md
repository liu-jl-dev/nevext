# Nevext Lua 实现测试计划

> 适用范围：`lua/nevext/nevext.lua`（Lua 5.4+，单文件，零第三方依赖）
> 测试代码目录：`lua/test/`
> **跨实现通用语料不在本文档**：六套实现共用的 60 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `lua/test/test_conformance.lua`。本文档只描述 Lua 私有的 `Impl` 套件。
> 文档最后更新：2026-11-10（v0.4.0：默认转义 nevext_escape；65 用例 / 481 断言通过）

## 1. 目标与非目标

**目标**

1. 用与其他五套实现相同的口径守住 Lua 实现的行为：`Conformance.*` 60 条语料 +
   dump 门禁（`test_main.lua --dump` 与 C++ 侧输出必须逐字节一致，diff 加
   `--strip-trailing-cr`）。
2. 覆盖 Lua 侧的工程性关注点：对象复用（reset）、终止性、大输入、字节保真。
3. 保持零依赖：不用 busted / lunit，`lua lua/test/test_main.lua` 一条命令即跑。

**非目标**

- 不测 LuaJIT / lua 5.1-5.3 等其他运行时（实现用到 5.3+ 的 `\u{XXXX}` 字符串
  转义与 5.4 的 `os.exit(code, true)`，因此目标运行时为 Lua 5.4+）。
- 不做性能基准：Lua 与 C++ 的性能差异属实现差异（见 `docs/zh-cn/test-plan.md` X3 的同款口径）。

## 2. 被测对象与 API 形状

单文件 `nevext/nevext.lua`（模块名 `nevext`，拷入工程即用，`require("nevext")`），
公开成员：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | 表，5 项，取值顺序与 C++ 一致（`Unexpected`=0 … `Sep`=4） |
| `Result` | 表，`Success`=0 / `Error`=1 |
| `Lexer` | `Lexer.new(code)`；方法 `next_token()`、`reset(code)`（冒号调用）；字段 `line`、`errors` |
| `Parser` | `Parser.new(lexer)`；方法 `parse_program(pause_when_error)`、`reset(lexer)`；字段 `errors` |
| `Token` / `Statement` / 解析结果 | 纯表：`{type, line, literal}` / `{line, strings}` / `{err, val}` |
| `expand_escapes` | 转义展开（模块级函数，返回 `(err, val, reason)`） |
| `VERSION` | 版本常量，与 CHANGELOG / git tag 同步 |
| `UNCLOSED_MESSAGE` / `INVALID_ESCAPE_MESSAGE` / `UNEXPECTED_MESSAGE` | 错误消息常量，与 C++ 逐字一致 |

与 C++ 的差异：`parse_program` 走**返回值**（`{ err, val }`），不用 C++ 的出参；
命名按 Lua 惯例 snake_case（`next_token` / `parse_program`），构造器统一 `Xxx.new(...)`
并用冒号调用（`lexer:next_token()`）。除此之外的行为契约与 C++ 逐条一致。

## 3. 实现要点（Lua 特有）

1. **字节模型。** Lua 的 string 是字节序列：非 ASCII 字符按 UTF-8 占多个字节，
   而 C++ / JS / C# 内部是 UTF-16 码元、Python 是码点。**本语法的全部结构字符
   （空白、`; " ' \` `/`、`\n`）都在 ASCII 区间**，UTF-8 的多字节字符按编码规则
   不可能包含这些字节，因此按字节扫描与按码元扫描行为完全等价；转义表里的
   映射也全是单字节。多字节内容原样保留，不做任何编解码（缺口 LU1）。
   写语料时注意：**H02 的 emoji 在 Lua 里写成 `\u{1F600}`**（Lua 5.3+ 会生成
   对应的 UTF-8 字节序列）。
2. **游标代替前移。** 与 JS / C# / Python 相同，「完整字符串 + `pos` 游标 +
   一次性切片」，行为一致且整体 O(n)。`reset` 同时复位游标。
3. **无 stdout 编码问题。** Lua 的 `io.write` 把字符串按原样字节写出，UTF-8
   内容（dump 的中文与 emoji）不需要像 Python 那样 reconfigure stdout；
   Windows 文本模式的 `\n → \r\n` 翻译与 C++ / C# 侧相同，靠
   `diff --strip-trailing-cr` 消化。
4. **错误消息逐字对齐 C++**（含句号后的空格，消息收敛为三个常量），一致性断言
   通过「消息 → (行号, 种类)」映射比较，与另五套相同。
5. **dump 的 `show()` 必须与 C++ 的 `EscapeVisible` 逐项一致**：六个具名控制字节
   （`\n` `\r` `\t` `\v` `\b` `\f`）、反斜杠翻倍、其余控制字节 `\xNN`（大写两位
   十六进制）；引号在 `show` 之后再转义（「先反斜杠、后引号」）。Lua 按字节遍历，
   多字节字符的每个字节都 ≥ 0x80，走「原样保留」分支，输出与按码点处理的
   Python 版逐字节相同。改动 dump 相关代码后必须重跑门禁。
6. **入口不经 `test.cmd` 也能自定位。** 各测试 / demo 文件用 `arg[0]` 推导自身
   所在目录并挂到 `package.path`，因此从任意工作目录用 `lua <路径>` 直接运行都成立。

## 4. 运行方式

```bash
# 全部用例（Conformance 60 条 + Impl 5 条）
lua lua/test/test_main.lua      # 或 lua\test.cmd

# 按名字子串筛选
lua lua/test/test_main.lua Conformance.C0

# 框架自检（show 转义与 C++ 侧一致）
lua lua/test/test_framework.lua

# dump 门禁（与 C++ 逐字节 diff，--strip-trailing-cr 不能省）
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(lua lua/test/test_main.lua --dump)
```

**当前基线（Lua 5.4.6，Windows）**：65 个用例 / 481 条断言全部通过
（Conformance 55 + Impl 5）。

## 5. 用例清单：`Impl` 套件（5 个，test_impl.lua）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| LU-01 | `ResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.line` 回到 1、两侧错误容器为空（对应 Python PY-01、GDScript IMP-02、C++ LEX-26 / PAR-18、JS JS-01、CS-01） |
| LU-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部**字节**前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile`（不死循环、不爆栈） |
| LU-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字节 |
| LU-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| LU-05 | `ArbitraryBytesPreserved` | `x \255 y "\254"`（非 UTF-8 合法序列的高位字节） | 高位字节在裸串 / 引号串（经语法层）中原样保留，无错误（对应 Python PY-05 的孤立代理项用例：各运行时按自己的元素模型保留） |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与其它实现相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |
| **LU1** | 字符串元素数按**字节**计：非 ASCII 字符在 Lua 里占多个字节，而 C++ / JS / C# 是 UTF-16 码元、GDScript / Python 是单码元 / 码点（对应文档 X2） | 已确认为实现差异，不影响契约 | 通用语料只断言字符串内容相等（H02），不断言长度；需要元素级语义时用 UTF-8 感知的库（如 utf8.len）在调用方处理 |

## 7. 目录结构

```
lua/
├── nevext/
│   └── nevext.lua           # 库本体：单文件、零依赖，拷入任意工程即用
├── test/
│   ├── test_framework.lua   # 零依赖自研框架（对齐 cpp/test 的设计取舍）
│   ├── test_conformance.lua # 60 条通用语料 + dump
│   ├── test_impl.lua        # Lua 私有的 Impl 套件（5 条）
│   └── test_main.lua        # 入口：--dump / 筛选 / 退出码
├── test.cmd                 # Windows 便捷入口（纯 ASCII 注释）
├── demo.cmd                 # Windows 便捷入口：跑示例
├── demo/
│   └── demo01.lua           # 可运行示例（六套 demo 同输入同格式）
└── docs/test-plan.md        # 本文档
```

> AI生成
