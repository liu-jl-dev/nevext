# Nevext C++ 实现测试计划

> 适用范围：`cpp/nevext/`（common.hpp / lexer.hpp / parser.hpp / nevext.hpp）
> 测试代码目录：`cpp/test/`
> **跨实现通用语料不在本文档**：C++ 与 GDScript 共用的 55 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `cpp/test/test_conformance.cpp`。本文档只描述 C++ 私有的 LEX / PAR / ERR 套件。
> 文档最后更新：2026-10-17（新增跨实现一致性套件，整个套件 109 用例 / 769 断言通过）

## 1. 目标与非目标

**目标**

1. 把 `docs/zh-cn/overview.md` 中规定的语法条款固化为可执行断言，规范变更或实现重构时能立刻发现回归。
2. 覆盖手写词法/语法分析最容易出错的地方：缓冲区推进是否为 0（死循环）、行号统计、转义处理、错误恢复、token 视图的生命周期。
3. 给出明确的运行入口（`ctest`），让这套测试能进 CI。

**非目标**

- 不测试 GDScript 实现（`gdscript/`），本计划只针对 C++ 实现；两套实现的**一致性**由
  `docs/zh-cn/test-plan.md` 定义的通用语料负责，C++ 侧的落地是 `test_conformance.cpp`。
- 不做语义层测试：Nevext 只规定如何解析字符串，不规定语义（`cpp/nevext` 也不提供语义 API）。
- 不做形式化的解析器一致性验证（差分测试 / goldens 文件），见第 9 节后续计划。

## 2. 被测对象

| 文件 | 职责 | 主要测试面 |
| --- | --- | --- |
| `cpp/nevext/common.hpp` | `TokenType`、`Result` 枚举 | 枚举取值约定（`Success == 0`） |
| `cpp/nevext/lexer.hpp` | 空白/注释跳过、裸串、引号串、分号、行号、错误收集 | 词法套件 `Lexer` |
| `cpp/nevext/parser.hpp` | 语句切分、转义展开、错误记录、错误恢复开关 | 语法套件 `Parser` |
| `cpp/nevext/nevext.hpp` | 统一入口（只 include parser） | 端到端用例间接覆盖 |

对外 API 面（测试用例只通过这层驱动，避免依赖私有成员函数）：

```cpp
Nevext::Lexer  lexer(input);            // input: std::u16string&，必须存活到解析结束
Nevext::Token  token = lexer.NextToken();   // {line, type, value(string_view)}
lexer.errors                            // std::vector<std::string>

Nevext::Parser parser(lexer);
Result r = parser.ParseProgram(prog, pauseWhenError);  // prog: std::vector<Statement>
parser.errors                           // std::vector<std::string>
```

## 3. 测试策略

1. **零第三方依赖的自研轻量框架。** 核心库是 header-only、零依赖，引入 GTest / Catch2 会破坏这一属性（也让其它平台移植、LLM 生成实现时必须先解决依赖）。框架实现见 `cpp/test/test_framework.hpp`，支持：自动注册用例、`-v` 详细模式、按用例名子串筛选、失败不中断、失败即非零退出码。
2. **黑盒为主 + 白盒边界。** 绝大多数用例从「输入源码 → 期望的 token 序列 / 语句列表 / 错误列表」驱动；少数用例针对实现细节做边界防护（注入nullptr不适用，本项目无指针 API），例如「每步都必须推进缓冲区」体现为 `EveryPrefixTerminates`、`PathologicalInputsTerminate` 两个终止性用例。
3. **断言双通道。** 错误既可能体现在 `Result` 返回值，也可能只体现在 `lexer.errors` / `parser.errors`（见已知缺陷 D1）。因此干净用例统一走 `ExpectClean()`：同时断言返回值与两个错误容器都为空。
4. **偏离项做成「行为快照」。** 发现规范未覆盖或实现疑似有缺陷的行为时，不写期望值正确但当前会失败的用例（那样 CI 长期红），而是断言**当前实际行为**，并在用例注释与本文档第 8 节标注编号 D1~D4。修复实现时需同步修改这些用例。
   **当前状态：D1 / D3 / D4 均已修复，D2 已由规范确认为「不是错误」，对应的快照用例已全部改回断言期望行为。**
5. **兼容.spec 与可移植。** 测试用例源码统一 UTF-8，测试数据用 `u"..."` 字面量（MSVC 已在顶层 CMakeLists 打开 `/utf-8`，MinGW/GCC 默认按 UTF-8 读取输入），非 ASCII 一律用 `\uXXXX` 写码元，避免源文件编码差异导致误判。

## 4. 目录结构与运行方式

```
cpp/
├── CMakeLists.txt            # 新增 nevext_test 目标与 add_test
├── build.cmd                 # 既有：cmake -B build && cmake --build build
├── test.cmd                  # 新增：跑单元测试（ctest）
├── docs/
│   └── test-plan.md          # 本文档
└── test/
    ├── test_framework.hpp    # 断言宏 / 用例注册 / 执行器（header-only）
    ├── test_util.hpp         # Run() / LexAll() / Expect*() 辅助
    ├── test_main.cpp         # main → RunAll，并处理 --dump
    ├── test_lexer.cpp        # 词法套件（LEX-01 ~ LEX-25）
    ├── test_parser.cpp       # 语法套件（PAR-01 ~ PAR-17）
    ├── test_error.cpp        # 错误处理与健壮性套件（ERR-01 ~ ERR-12）
    └── test_conformance.cpp  # 跨实现通用语料（Conformance.A01 ~ H04，见 docs/zh-cn/test-plan.md）
```

**运行**

```bash
# 方式一：ctest（推荐，CI 用）
cmake -B build && cmake --build build
ctest --test-dir build -C Debug --output-on-failure      # cmake >= 3.20
# 或
cd build && ctest -C Debug --output-on-failure

# 方式二：直接运行（支持筛选，调试单个用例最快）
./build/test/nevext_test            # 全部用例
./build/test/nevext_test Lexer      # 只跑词法套件
./build/test/nevext_test -v Error   # 详细输出 + 筛选
```

**关闭测试构建**：`cmake -B build -DNEVEXT_BUILD_TESTS=OFF`。

**智能体/本地快速验证**（无需 cmake，与仓库既有约定一致）：

```bash
g++ -std=c++20 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 \
    -Icpp/nevext -Icpp/test \
    -o nevext_test.exe \
    cpp/test/test_main.cpp cpp/test/test_lexer.cpp cpp/test/test_parser.cpp \
    cpp/test/test_error.cpp cpp/test/test_conformance.cpp
./nevext_test.exe
```

**当前基线（MinGW GCC 15.2，C++20）**：109 个用例 / 769 条断言全部通过。
其中 55 个是跨实现通用语料（`Conformance.*`），由 `docs/zh-cn/test-plan.md` 定义。

## 5. 需求 → 用例追溯矩阵

| 规范条款（docs/zh-cn/overview.md） | 对应用例 |
| --- | --- |
| `prog : stmt*`（程序为语句列表） | PAR-01、PAR-05、PAR-10 |
| 逗号等同空白 | LEX-02、LEX-04、PAR-01、PAR-02 |
| 连续/多余逗号不产生空字符串条目 | PAR-02 |
| 空语句不保留 | PAR-03 |
| 最后一个语句可省略分号 | PAR-04、PAR-13 |
| `raw_str` 规则（不含空白、逗号、分号、引号，遇注释终止） | LEX-03、LEX-15、LEX-16 |
| `sq_str` / `dq_str` | LEX-05、LEX-06、LEX-08、LEX-09、LEX-17、LEX-18 |
| 单引号串内可转义 `'`，双引号串内可转义 `"` | PAR-06 |
| `raw_str` 不转义 | LEX-07、PAR-07 |
| 引号串可跨行，结果含换行符；串内换行计入行号 | LEX-23、LEX-24、LEX-25、PAR-17 |
| 转义字符表（`\\ \' \" \n \r \t \v \b \f`） | PAR-06 |
| 单行注释 `//` | LEX-11、PAR-05 |
| 多行注释 `/* */` | LEX-12、LEX-13、PAR-05 |
| 未闭合的多行注释不是错误（延伸到输入结束） | ERR-03 |
| 内部约定：`Result::Success == 0` | PAR-16 |
| 内部约定：`Token::value` 为输入缓冲区视图 | ERR-11 |

## 6. 用例清单

### 6.1 词法套件 `Lexer`（25 个，test_lexer.cpp）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| LEX-01 | `EmptyInputGivesEof` | 空串 | 首个 token 即 `EndOfFile`，line=1，无错误 |
| LEX-02 | `WhitespaceAndCommaAreSkipped` | 空格/制表/换行/垂直制表/换页/回车/逗号 | 全部跳过，只剩 EOF（退格**不属于**空白，见通用语料 D10） |
| LEX-03 | `RawStringBasic` | `abc` | 一个 `RawStr`，值为 `abc` |
| LEX-04 | `CommaSeparatesRawStrings` | `a,b` | 两个 `RawStr` |
| LEX-05 | `DoubleQuotedString` | `"hello"` | `QuotedStr`，值不含引号 |
| LEX-06 | `SingleQuotedString` | `'hello'` | `QuotedStr` |
| LEX-07 | `EscapeSequenceKeptRawByLexer` | `"a\nb"`（字面反斜杠 n） | 词法层保留 4 个原始码元，不做展开 |
| LEX-08 | `EmptyQuotedStrings` | `"" ''` | 两个空内容的 `QuotedStr` |
| LEX-09 | `QuotedStringPreservesSpaces` | `"a b" c` | 引号内空格属于内容 |
| LEX-10 | `SeparatorToken` | `a;b` | 产出独立 `Sep` token |
| LEX-11 | `LineCommentSkippedAndCounted` | `// comment\na` | 注释丢弃，`a` 在 line 2 |
| LEX-12 | `BlockCommentSkippedAndCounted` | `/*\n\n*/ a` | 注释内换行计入，`a` 在 line 3 |
| LEX-13 | `BlockCommentInsideOneLine` | `/*x*/a` | 单行注释不换行，行号保持 1 |
| LEX-14 | `CrlfCountsAsOneLine` | `a\r\nb` | CRLF 只推进行号 1（见 D3 旁注） |
| LEX-15 | `RawStringStopsAtLineComment` | `a//x\nb` | 裸串在 `//` 处结束，不含注释 |
| LEX-16 | `RawStringStopsAtBlockComment` | `a/*x*/b` | 裸串在 `/*` 处结束 |
| LEX-17 | `AdjacentTokensNeedNoSpace` | `"a"b` | 引号串与裸串相邻无需空白 |
| LEX-18 | `OtherQuoteInsideQuotedString` | `'"' "'"` | 异种引号无需转义 |
| LEX-19 | `UnclosedStringProducesUnexpectedAndRecovers` | `"abc` | `Unexpected` + 1 条 unclosed 错误 + 剩余内容降级为 `RawStr` |
| LEX-20 | `TrailingBackslashEscapesClosingQuote` | `"abc\"` | 连续两个 `Unexpected`，两条 unclosed 错误，终止于 EOF |
| LEX-21 | `NonAsciiRawString` | `海上生明月 天涯共此时` | 按 UTF-16 码元原样保留（各 5 码元） |
| LEX-22 | `LoneQuoteTerminates` | `"` | 报错且能正常到达 EOF（不死循环） |
| LEX-23 | `QuotedStringCanCrossLines` | `"a<换行>b" c` | 值含真实换行；后续 token 位于第 2 行 |
| LEX-24 | `LineCountingContinuesAfterMultiLineQuotedString` | `"a<换行>b"<换行>c;` | 串内换行 + 串后换行累计，`c` 在第 3 行 |
| LEX-25 | `EscapedNewlineInsideQuotedStringCountsAsLine` | `"a\<换行>b"<换行>c` | 被反斜杠转义的换行也是物理换行，同样计数 |

### 6.2 语法套件 `Parser`（17 个，test_parser.cpp）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| PAR-01 | `SpecExampleBasic` | `windowSize 1280, 720;` | 1 语句 / 3 字符串 / 无错误 / `Success` |
| PAR-02 | `SpecExampleRedundantCommas` | `windowSize 1280, , 720 ,;` | 与 PAR-01 完全相同 |
| PAR-03 | `EmptyStatementsAreDropped` | `;; a ;;; b ;;` | 只保留 `a`、`b` 两条语句 |
| PAR-04 | `LastStatementWithoutSemicolon` | `a b; c d` | 两条语句，末句无需分号 |
| PAR-05 | `OnlyCommentsAndWhitespace` | 纯注释 / 纯空白 / 空串 | 空程序 + `Success` + 无错误 |
| PAR-06 | `AllSupportedEscapes` | 转义表中全部 9 项 | 展开为 9 个目标字符的正确序列 |
| PAR-07 | `RawStringIsNotUnescaped` | `a\nb`（裸串） | 保留字面的 `\` 与 `n` |
| PAR-08 | `MixedStringKindsInOneStatement` | `'x' y "z";` | 同一语句内三种字符串混用 |
| PAR-09 | `EmptyQuotedStringBecomesEmptyEntry` | `"" '' x;` | 产出空字符串条目且不丢位 |
| PAR-10 | `StatementLineNumbers` | `a;\nb;\n\n c d;` | 语句行号分别为 1 / 2 / 4 |
| PAR-11 | `InvalidEscapeReportsError` | `"a\qb"; c` | `Result::Error` + `invalid escape character.` + 语句保留前缀 `a` + 默认停止后续解析 |
| PAR-12 | `ContinueParsingWhenErrorRecoveryEnabled` | `a;\n"b\qc";\nd;`（pause=false） | 返回 `Error` 但三条语句都被产出 |
| PAR-13 | `MultiLineStatement` | 跨行语句 | 字符串列表跨行合并 |
| PAR-14 | `NonAsciiRoundTrip` | 中文作为裸串与引号串内容 | 与输入码元一致 |
| PAR-15 | `Demo01InputRegression` | `demo/demo01.cpp` 的输入 | 与既有 demo 行为一致（2 语句，行号 2 与 6） |
| PAR-16 | `ResultSuccessIsZero` | — | `Success == 0`、`Error == 1`，可直接写作 `if (result)` |
| PAR-17 | `StatementLineAfterMultiLineQuotedString` | `"a<换行>b"; c;<换行>d;` | 三条语句行号分别为 1 / 2 / 3（D3 回归防线） |

### 6.3 错误处理与健壮性套件 `Error`（12 个，test_error.cpp）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| ERR-01 | `UnclosedStringStopsAndRecordsErrors` | `a; "bc`（pause=true） | 停止解析，两侧错误各 1 条，**返回 `Error`**（D1 修复后） |
| ERR-02 | `UnclosedStringRecoveryWhenNotPaused` | `a; "bc; d`（pause=false） | 继续解析出 3 条语句，错误仍被记录，**返回 `Error`**（D1 修复后） |
| ERR-03 | `UnterminatedBlockCommentIsNotAnError` | `a /* xx`、`a; /* oops<换行>b c;<换行>d;` | 注释未闭合**不是错误**：两个容器都为空、返回 `Success`，其后内容全部被当作注释丢弃 |
| ERR-04 | `ErrorMessageFormat` | 未闭合串（第 1 行 / 第 2 行） | 消息格式为 `line N: unclosed string literal. `，行号随 token 走 |
| ERR-05 | `SameLineErrorsAreAllReported` | 同一行两处非法转义 | 两条消息全部保留（D4：按行去重已移除） |
| ERR-06 | `ErrorsOnDifferentLinesAreAllReported` | 第 1、2 行各一处非法转义 | 两条消息，顺序与源码一致 |
| ERR-07 | `PathologicalInputsTerminate` | 只有引号、只有反斜杠、`/*`、`*/`、`//` 等 12 组 | 每组都能在阈值内扫到 `EndOfFile` |
| ERR-08 | `EveryPrefixTerminates` | 恶意串 `a "b\"\c' /* xx //<换行>y ;\t'z"*/ ;` 的全部前缀 | 每个前缀都能扫到 EOF，不死循环、不崩溃 |
| ERR-09 | `LongRawTokenIsComplete` | 10000 个 `x` + `;` | 单个 `RawStr` 不被截断；解析结果与输入一致 |
| ERR-10 | `ManyStatementsAreAllParsed` | 2000 条语句 | 数量、内容、行号全部正确 |
| ERR-11 | `TokenValuesRemainValidWhileBufferAlive` | `alpha beta; gamma` | 缓冲区存活时早先 token 的 `string_view` 仍可读 |
| ERR-12 | `UnclosedStringSpanningLinesReportsLastLine` | `"a<换行>b`（跨行未闭合） | 词法错误报在串结束行（第 2 行），语法错误报在串起始行（第 1 行），返回 `Error` |

## 7. 断言覆盖率

以「对外 API × 输入类别」计，`cpp/nevext` 当前可达状态覆盖情况：

| 维度 | 覆盖情况 |
| --- | --- |
| `Lexer::NextToken` 全部分支（`Unexpected` / `EndOfFile` / `RawStr` / `QuotedStr` / `Sep`） | 已覆盖 |
| `skipWs` 的空白集合、行注释、多行注释 | 已覆盖（含 CRLF 组合） |
| `scanQuotedStr` 成功 / 未闭合 / 转义吞掉闭合引号 | 已覆盖 |
| 行号统计（空白、两种注释、CRLF、引号串内部、被转义的换行） | 已覆盖（LEX-11~LEX-14、LEX-23~LEX-25、PAR-10、PAR-17、ERR-12） |
| `scanRawStr` 的终止字符集与注释提前终止 | 部分覆盖：注释、`;`、`"`、`'` 已覆盖；`\b`/`\v`/`\f` 作为终止符未单独覆盖（低风险） |
| `Parser::ParseProgram` 的 `pauseWhenError` 两种取值 | 已覆盖 |
| `Parser::parseStatement` 的全部 token 分支 | 已覆盖（`Unexpected` 分支由 ERR-01 覆盖） |
| `Parser::escape` 成功路径与两种失败路径（串尾反斜杠、非法转义） | 部分覆盖：非法转义已覆盖，串尾落在反斜杠上的路径未覆盖（该状态被词法层提前拦截，见第 8 节备注） |
| `common.hpp` 中 `UnknownError` / `UnclosedStringError` | 未覆盖：二者均为内部返回码，不会经 `ParseProgram` 向外暴露 |

## 8. 缺陷处置记录

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| **D1** | 语法错误未反映到返回值：`ParseProgram` 顶层遇到 `Unexpected` token 时只写 `parser.errors`，未把 `result` 置为 `Error`（未闭合字符串会「返回成功但内容缺失」） | ✅ 已修复（2026-10-17） | `parser.hpp` 顶层 `else` 分支补 `result = Result::Error;`。ERR-01 / ERR-02 已改为断言 `Error` |
| **D2** | 未闭合的多行注释被静默吞掉，既不报错也不保留后续内容 | ✅ 已定为规范（2026-10-17） | 不是缺陷：`docs/zh-cn/overview.md` 已明文规定「未闭合的多行注释不是错误，一直延伸到输入结束」。ERR-03 改为断言这一规定；`guide-cpp.md` 的「非错误的情况」也做了说明 |
| **D3** | 跨行引号串不推进行号，后续 token / 语句行号整体偏小 | ✅ 已修复（2026-10-17） | `lexer.hpp` 的 `scanQuotedStr` 对串内每个 `\n`（含被反斜杠转义的换行）累加 `line`。新增 LEX-24 / LEX-25 / PAR-17 / ERR-12 作为回归防线 |
| **D4** | 同一行内的多处错误只报第一条（`Parser::error` 按 `lastErrorLine` 去重） | ✅ 已修复 | 去重逻辑已移除，同行的多处错误全部记录。ERR-05 已改为断言两条消息 |
| **B1**（非缺陷，契约） | `Token::value` 是 `std::u16string_view`，指向调用方传入的缓冲区 | — | 由 ERR-11 固化契约；`docs/zh-cn/guide-cpp.md` 的生命周期表已说明 |

**修复后的连带影响**

- 未闭合字符串的 `Result` 由 `Success` 变为 `Error`，依赖「只看返回值」的调用方现在能正确感知错误（`demo/demo01.cpp` 的 `if (result)` 分支此前不会命中，现在会）。
- 引号串内换行计入行号后，错误消息里的行号会指向字符串**结束**所在行，而 `unexpected token.` 仍指向字符串**起始**行（ERR-12 固化了这一语义）。

## 9. 后续计划

按优先级：

1. ~~修复 D1~~（已完成）、~~确认 D2 / D3 / D4~~（已完成，见第 8 节）。
2. ~~引入 golden 测试~~（已完成）：改为「跨实现通用语料」形态落地在 `test_conformance.cpp`，
   语料与期望值沉淀在 `docs/zh-cn/test-plan.md`，GDScript 侧跑同一份，另有 `--dump` 支持逐行 diff。
4. **结构化模糊测试**：现有 `EveryPrefixTerminates` 只保证「不死循环」，可升级为「随机生成串 → 要么成功解析、要么产出至少一条错误」，用于搜索 D1 一类的静默失败。
5. **性能基线**：`LongRawTokenIsComplete` / `ManyStatementsAreAllParsed` 目前只做正确性；可改为打印耗时并设阈值，防止后续改动引入退化。
6. **CI 接入**：GitHub Actions 上加 `cmake -B build && cmake --build build && ctest --output-on-failure`，覆盖 MSVC / GCC / Clang 三套编译器（同时可顺带验证源文件 UTF-8 编码假设是否成立）。
