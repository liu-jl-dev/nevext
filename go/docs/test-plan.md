# Nevext Go 实现测试计划

> 适用范围：`go/nevext.go`（Go 1.21+，单文件，零第三方依赖，module 根目录）
> 测试代码目录：`go/test/`
> **跨实现通用语料不在本文档**：各实现共用的 60 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `go/test/test_conformance.go`。本文档只描述 Go 私有的 `Impl` 套件。
> 文档最后更新：2026-11-10（v0.4.0：默认转义 nevext_escape；65 用例 / 481 断言通过）

## 1. 目标与非目标

**目标**

1. 用与其他实现相同的口径守住 Go 实现的行为：`Conformance.*` 60 条语料 +
   dump 门禁（`go run ./test --dump` 与 C++ 侧输出必须逐字节一致，diff 加
   `--strip-trailing-cr`）。
2. 覆盖 Go 侧的工程性关注点：对象复用（Reset）、终止性、长裸串、大输入、
   任意字节保真。
3. 保持零依赖：不用 `testing` 之外的任何框架（连标准库 `testing` 也不用，
   自研框架与 `cpp/test` 的设计取舍一致），`go run ./test` 一条命令即跑。

**非目标**

- 不做性能基准：与 C++ 的性能差异属实现差异（`docs/zh-cn/test-plan.md` X3 的同款口径）。
- 不测 gopherjs / tinygo 等其他运行时。

## 2. 被测对象与 API 形状

单文件 `nevext.go`（module 根目录，包名 `nevext`，import 只有一层；拷入任意工程即用），公开成员：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | `Unexpected=0 … Sep=4`，取值顺序与 C++ 一致 |
| `Result` | `Success=0 / Error=1` |
| `Token` | `Type / Line / Literal` |
| `Statement` | `Line / Strings` |
| `ParseResult` | `Err / Val`（`ParseProgram` 的返回值，语句列表走返回值） |
| `Lexer` | `NewLexer(code)`；方法 `NextToken()`、`Reset(code)`；公开字段 `Code / Pos / Line / Errors` |
| `Parser` | `NewParser(lexer)`；方法 `ParseProgram(pauseWhenError)`、`Reset(lexer)`；公开字段 `Lexer / Errors` |
| `ExpandEscapes` | 转义展开（包级函数，返回 `(err, val, reason)`） |
| `Version` | 版本常量，与 CHANGELOG / git tag 同步 |
| `UnclosedMessage` / `InvalidEscapeMessage` / `UnexpectedMessage` | 错误消息常量，与 C++ 逐字一致 |

与 C++ 的差异：`ParseProgram` 走**返回值**（`ParseResult.Val`），不用 C++ 的出参；
错误容器为公开字段（Go 惯例）；命名按 Go 惯例 PascalCase 导出。除此之外的
行为契约与 C++ 逐条一致。

## 3. 实现要点（Go 特有）

1. **字节模型。** Go 的 string 是 UTF-8 字节序列（与 Lua 同款论证）：本语法的
   全部结构字符（空白、`; " ' \` `/`、`\n`）都在 ASCII 区间，UTF-8 多字节字符
   不可能包含它们，按字节扫描与按码元扫描行为完全等价；转义表全是单字节映射。
   多字节内容原样保留（缺口 GO1）。写语料时 **H02 的 emoji 写 `\U0001F600`**
   （Go 字面量通用转义，运行期落成 4 字节 UTF-8）。
2. **游标代替前移。** 与 JS / C# / Python / Lua 相同，「完整字符串 + `Pos`
   游标 + 一次性切片」，行为一致且整体 O(n)。`Reset` 同时复位游标。
3. **无 stdout 编码问题。** `fmt.Println` 按原样字节写出 UTF-8（Go 的 stdout
   不经过任何本地编码转换），中文与 emoji 的 dump 不需要 Python 那样的
   reconfigure；Windows 上 Go 写 `\n` 不做文本翻译，输出本身就是 LF。
4. **错误消息逐字对齐 C++**（含句号后的空格，消息收敛为三个常量），一致性断言
   通过「消息 → (行号, 种类)」映射比较。
5. **dump 的 `show()` 按字节遍历**：六个具名控制字节、反斜杠翻倍、其余控制
   字节 `\xNN`（大写两位十六进制）；≥0x80 的字节原样透传——多字节 UTF-8
   序列逐字节拼回后与按码点处理的 Python 版输出逐字节相同。
6. **`go run` 需要模块上下文。** `go.mod` 在 `go/` 目录（module 名 `nevext`，
   仅为仓库内测试存在；库本体拷走时不需要它），`test.cmd` / `demo.cmd` 里
   `pushd "%~dp0"` 后再 `go run ./test` / `go build ./demo`。
7. **demo 先构建到 `%TEMP%` 再运行**：让 demo 进程跑在调用方 cwd，
   相对路径参数保持语义，构建日志也不会混进输出。

## 4. 运行方式

```bash
# 全部用例（Conformance 60 条 + Impl 5 条）
go\test.cmd                      # 等价于 cd go && go run ./test

# 按名字子串筛选 / dump 门禁
cd go && go run ./test Conformance.C0
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(cd go && go run ./test --dump)
```

**当前基线（Go 1.27.3，Windows）**：65 个用例 / 481 条断言全部通过
（Conformance 55 + Impl 5）。

## 5. 用例清单：`Impl` 套件（5 个，test_impl.go）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| GO-01 | `ResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`Reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.Line` 回到 1、两侧错误容器为空（对应 PY-01 / LU-01 / CS-01 / C++ LEX-26 / PAR-18） |
| GO-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部字节前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile` |
| GO-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字节 |
| GO-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| GO-05 | `ArbitraryBytesPreserved` | `x \xC3 y`（孤立高位字节，非合法 UTF-8 序列） | 高位字节在裸串中原样保留，后续裸串不受影响，无错误（对应 PY-05 孤立代理项：Go string 可容纳任意字节，这是 Rust / Python 的 &str / str 做不到的） |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与其它实现相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |
| **GO1** | 字符串元素数按**字节**计：非 ASCII 字符在 Go 里占多个字节，而 C++ / JS / C# 是 UTF-16 码元、GDScript / Python 是单码元 / 码点（对应文档 X2） | 已确认为实现差异，不影响契约 | 通用语料只断言字符串内容相等（H02），不断言长度；需要元素级语义时用 `unicode/utf8` 在调用方处理 |

## 7. 目录结构

```
go/
├── go.mod                   # 仅仓库内测试用；库本体拷走时不需要
├── nevext/
│   └── nevext.go            # 库本体：单文件、零依赖，拷入任意工程即用
├── test/
│   ├── test_framework.go    # 零依赖自研框架（对齐 cpp/test 的设计取舍）
│   ├── test_conformance.go  # 60 条通用语料 + dump
│   ├── test_impl.go         # Go 私有的 Impl 套件（5 条）
│   └── test_main.go         # 入口：--dump / 筛选 / 退出码
├── test.cmd                 # Windows 便捷入口（纯 ASCII 注释）
├── demo.cmd                 # Windows 便捷入口：跑示例
├── demo/
│   └── demo01.go            # 可运行示例（八套 demo 同输入同格式）
└── docs/test-plan.md        # 本文档
```

> AI生成
