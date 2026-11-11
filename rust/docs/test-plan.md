# Nevext Rust 实现测试计划

> 适用范围：`rust/nevext/nevext.rs`（Rust 1.70+ / edition 2021，单文件，零第三方依赖）
> 测试代码目录：`rust/test/`
> **跨实现通用语料不在本文档**：各实现共用的 60 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `rust/test/test_conformance.rs`。本文档只描述 Rust 私有的 `Impl` 套件。
> 文档最后更新：2026-11-10（v0.4.0：默认转义 nevext_escape；65 用例 / 481 断言通过）

## 1. 目标与非目标

**目标**

1. 用与其他实现相同的口径守住 Rust 实现的行为：`Conformance.*` 60 条语料 +
   dump 门禁（`cargo run --bin nevext_test -- --dump` 与 C++ 侧输出必须逐字节
   一致，diff 加 `--strip-trailing-cr`）。
2. 覆盖 Rust 侧的工程性关注点：复用（reset）、终止性、长裸串、大输入、
   多字节字符保真。
3. 保持零依赖：不用 cargo test 宏体系与任何外部 crate，自研框架与
   `cpp/test` 的设计取舍一致。

**非目标**

- 不做性能基准：与 C++ 的性能差异属实现差异（`docs/zh-cn/test-plan.md` X3 的同款口径）。
- 不支持 no_std：库用到 `String` / `format!`（std），no-core 场景不在范围内。

## 2. 被测对象与 API 形状

单文件 `nevext/nevext.rs`（crate 名 `nevext`，拷入工程即用，`[lib] path` 指定），
公开成员：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | 枚举，`Unexpected=0 … Sep=4`，取值顺序与 C++ 一致 |
| `Status` | 枚举，`Success=0 / Error=1`（**不叫 Result**，避免与 `std::result::Result` 混淆） |
| `Token` | `kind / line / literal` |
| `Statement` | `line / strings` |
| `ParseResult` | `err / val`（`parse_program` 的返回值，语句列表走返回值） |
| `Lexer` | `Lexer::new(code)`；方法 `next_token()`、`reset(code)`；公开字段 `code / pos / line / errors` |
| `Parser` | `Parser::new()`；方法 `parse_program(lexer, pause_when_error)`、`reset()`；公开字段 `errors`。**不绑定词法器**——借用检查器友好的惯用做法是每次调用显式传入 `&mut Lexer` |
| `expand_escapes` | 转义展开（crate 级函数，返回 `(err, val, reason)`） |
| `VERSION` | 版本常量，与 CHANGELOG / git tag 同步 |
| `UNCLOSED_MESSAGE` / `INVALID_ESCAPE_MESSAGE` / `UNEXPECTED_MESSAGE` | 错误消息常量，与 C++ 逐字一致 |

与 C++ 的差异：`parse_program` 走**返回值**；错误容器为公开字段；Parser 与
Lexer 的绑定关系改为调用期传参。除此之外的行为契约与 C++ 逐条一致。

## 3. 实现要点（Rust 特有）

1. **字节模型。** Rust 的 `&str` 是 UTF-8 字节序列（与 Lua / Go 同款论证）：
   结构字符全在 ASCII 区间，按字节扫描（`as_bytes()` + 字节下标）与按码元
   扫描行为完全等价；转义表全是单字节映射。多字节内容原样保留（缺口 RS1）。
   **语言限制**：`&str` 必须是合法 UTF-8，孤立代理项与非 UTF-8 字节序列无法
   表达（缺口 RS2，对应 Python PY-05 / Go GO-05 的场景在 Rust 侧不可构造）。
2. **写语料时**：emoji 写 `\u{1F600}`；**Rust 字面量没有 `\b` 转义**（`\v`
   也没有），退格 / 垂直制表写 `\x08` / `\x0b`。D10 即踩过这个坑。
3. **游标代替前移。** 「完整字符串 + `pos` 游标 + 一次性切片」，行为一致且
   整体 O(n)。`reset` 同时复位游标。
4. **无 stdout 编码问题。** `println!` 按原样字节写出 UTF-8；Windows 上写
   `\n` 不做文本翻译，输出本身就是 LF。
5. **错误消息逐字对齐 C++**（含句号后的空格），一致性断言通过
   「消息 → (行号, 种类)」映射比较。C++ / Python 的「undefined token type」
   兜底分支在 Rust 里不需要——`match` 对五个 `TokenType` 取值要求穷尽。
6. **dump 的 `show()` 按字节遍历**：六个具名控制字节、反斜杠翻倍、其余控制
   字节 `\xNN`（大写两位十六进制）；≥0x80 的字节原样透传，多字节 UTF-8
   序列逐字节拼回后与 Python 版输出逐字节相同。
7. **测试注册走 `Box<dyn Fn>`**：框架注册表是 `Mutex<Vec<(String, Box<dyn Fn
   + Send>)>>`，闭包捕获语料表引用即可；语料表本体放在 `OnceLock` 全局静态。
   panic 用 `catch_unwind` 兜住计入失败（对应 Python 的 try/except）。
8. **demo / 测试二进制经 Cargo.toml 的 `[[bin]]` 挂接**（`nevext_test` 与
   `demo01`）；`demo.cmd` 先 `cargo build` 再直接运行 exe，让 demo 进程跑在
   调用方 cwd，构建日志也不会混进输出。

## 4. 运行方式

```bash
# 全部用例（Conformance 60 条 + Impl 5 条）
rust\test.cmd                    # 等价于 cd rust && cargo run -q --bin nevext_test

# 按名字子串筛选 / dump 门禁
cd rust && cargo run -q --bin nevext_test -- Conformance.C0
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(cd rust && cargo run -q --bin nevext_test -- --dump)
```

**当前基线（rustc 1.91.0，Windows）**：65 个用例 / 481 条断言全部通过，
0 警告（Conformance 55 + Impl 5）。

## 5. 用例清单：`Impl` 套件（5 个，test_impl.rs）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| RS-01 | `ResetIsReusable` | 同一 `Parser` 实例 + `Lexer::reset`，解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.line` 回到 1、两侧错误容器为空（对应 PY-01 / LU-01 / GO-01 / CS-01 / C++ LEX-26 / PAR-18） |
| RS-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部字节前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile` |
| RS-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字节 |
| RS-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| RS-05 | `MultibyteCharsPreserved` | `x é中😀 y`（2+3+4 字节的多字节序列） | 多字节字符在裸串中原样保留；`len()` 计 9 字节（缺口 RS1）；无错误 |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与其它实现相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |
| **RS1** | 字符串元素数按**字节**计：非 ASCII 字符在 Rust 里占多个字节，而 C++ / JS / C# 是 UTF-16 码元、GDScript / Python 是单码元 / 码点（对应文档 X2） | 已确认为实现差异，不影响契约 | 通用语料只断言字符串内容相等（H02），不断言长度；需要元素级语义时用 `str::chars().count()` 在调用方处理 |
| **RS2** | `&str` 必须是合法 UTF-8：孤立代理项、非 UTF-8 字节序列无法表达（Python 的 str 允许孤立代理项、Go 的 string 允许任意字节） | 语言限制，不修复 | 输入保证为合法 UTF-8 时行为与其它实现一致 |

## 7. 目录结构

```
rust/
├── Cargo.toml               # [lib] path 指向 nevext/nevext.rs；[[bin]] 挂 test / demo
├── nevext/
│   └── nevext.rs            # 库本体：单文件、零依赖，拷入任意工程即用
├── test/
│   ├── test_framework.rs    # 零依赖自研框架（对齐 cpp/test 的设计取舍）
│   ├── test_conformance.rs  # 60 条通用语料 + dump
│   ├── test_impl.rs         # Rust 私有的 Impl 套件（5 条）
│   └── test_main.rs         # 入口：--dump / 筛选 / 退出码
├── test.cmd                 # Windows 便捷入口（纯 ASCII 注释）
├── demo.cmd                 # Windows 便捷入口：跑示例
├── demo/
│   └── demo01.rs            # 可运行示例（八套 demo 同输入同格式）
└── docs/test-plan.md        # 本文档
```

> AI生成
