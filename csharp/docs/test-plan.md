# Nevext C# 实现测试计划

> 适用范围：`csharp/nevext/Nevext.cs`（.NET，C# 单文件，零第三方依赖）
> 测试代码目录：`csharp/test/`
> **跨实现通用语料不在本文档**：四套实现共用的 55 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `csharp/test/TestConformance.cs`。本文档只描述 C# 私有的 `Impl` 套件。
> 文档最后更新：2026-10-30（初版，60 用例 / 449 断言通过）

## 1. 目标与非目标

**目标**

1. 用与 C++ / GDScript / JavaScript 相同的口径守住 C# 实现的行为：`Conformance.*` 55 条
   语料 + dump 门禁（`nevext_test.exe --dump` 与 C++ 侧输出必须逐字节一致）。
2. 覆盖 C# 侧的工程性关注点：对象复用（Reset）、终止性、大输入、码元保真。
3. 保持零依赖：单一 csproj、无 NuGet 引用，`dotnet build` 即可编译。

**非目标**

- 不测 Unity / Mono 运行时差异（实现只用最基础的 BCL API，理论上可直接搬进
  Unity 工程，但不在本计划的验证范围内）。
- 不做性能基准：C# 与 C++ 的性能差异属实现差异（见 `docs/zh-cn/test-plan.md` X3 的同款口径）。

## 2. 被测对象与 API 形状

单文件 `nevext/Nevext.cs`（`namespace Nevext`，拷入任意工程即用），公开类型：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | 5 个枚举项，取值顺序与 C++ 一致（`Unexpected`=0 … `Sep`=4） |
| `Result` | `Success`=0 / `Error`=1 |
| `Lexer` | `new Lexer(input)`、`NextToken()`、`Reset(input)`；`Line`、`Errors: List<string>` |
| `Parser` | `new Parser(lexer)`、`ParseProgram(pauseWhenError=true)`、`Reset(lexer)`；`Errors: List<string>` |
| `Statement` / `ParseResult` / `Token` | 产物类型 |
| `Parser.ExpandEscapes` | 转义展开（公开静态，仅供测试与工具使用） |

与 C++ 的差异：`ParseProgram` 走**返回值**（`ParseResult { Err, Val }`），不用 C++ 的出参；
命名风格与 C++ 参考实现同为 PascalCase。除此之外的行为契约与 C++ 逐条一致。

## 3. 实现要点（C# 特有）

1. **字符串即码元。** C# 的 `string` 天然是 UTF-16 码元序列（`char` = 1 个码元），
   与 C++ 的 `std::u16string` 同构：代理对、孤立代理项都按码元原样保留
   （`Impl.LoneSurrogatePreserved` 守护；GDScript 的 UTF-32 模型无法表达孤立代理项）。
   源文件统一 UTF-8，非 ASCII 语料一律写 `\uXXXX` 码元，与文档约定一致。
   注意 C# 的 `\uXXXX` 固定 4 位十六进制，没有 JS 那种 `\x` 变长歧义，写码元更省心。
2. **游标代替切片。** C++ 用 `string_view` 前移，GDScript 每步 `substr()`（超线性）；
   C# 用「完整字符串 + `_pos` 游标 + `Substring` 一次性取 token」，行为一致且整体 O(n)。
   `Reset` 同时复位游标。
3. **错误消息逐字对齐 C++**（含句号后的空格，消息常量收敛为 `Lexer.UnclosedMessage` 等
   三个 `const`），一致性断言通过「消息 → (行号, 种类)」映射比较，与另三套相同。
4. **dump 的 `Show()` 必须与 C++ 的 `EscapeVisible` 逐项一致**：六个具名控制字符
   （`\n` `\r` `\t` `\v` `\b` `\f`）、反斜杠翻倍、其余控制字符 `\xNN`（大写两位十六进制）；
   引号在 Show 之后再转义（「先反斜杠、后引号」）。JS 侧第一版就漏过这两类，
   靠 dump 门禁抓出——**改动 dump 相关代码后必须重跑门禁**。
5. **控制台编码。** dump 含中文（H04），Windows 控制台默认代码页不是 UTF-8，
   `TestMain.Main` 开头显式设 `Console.OutputEncoding = Encoding.UTF8`。
   源文件无 BOM 时 Roslyn 先按 UTF-8 严格解码，失败才退回系统代码页，
   因此纯 UTF-8（无 BOM）源文件在本仓库是安全的。

## 4. 运行方式

```bash
# 全部用例（Conformance 55 条 + Impl 5 条）
csharp\test.cmd                       # 构建后直接跑 exe，输出不带构建噪声
dotnet run --project csharp           # 等价（stdout 会混入构建消息）

# 按名字子串筛选
csharp\test.cmd Conformance.C0

# dump 门禁（与 C++ 逐字节 diff；先 dotnet build -c Release csharp）
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(csharp/bin/Release/net10.0/nevext_test.exe --dump)
```

**当前基线（.NET SDK 10.0.401，net10.0，Release）**：60 个用例 / 449 条断言全部通过
（Conformance 55 + Impl 5）。

## 5. 用例清单：`Impl` 套件（5 个，TestImpl.cs）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| CS-01 | `ResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`Reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.Line` 回到 1、两侧错误容器为空（对应 GDScript IMP-02、C++ LEX-26 / PAR-18、JS JS-01） |
| CS-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile`（不死循环、不爆栈） |
| CS-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字符 |
| CS-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| CS-05 | `LoneSurrogatePreserved` | `x <孤立高代理> y "<孤立低代理>"` | 孤立代理项按码元原样保留，无错误（UTF-16 系特有） |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与其它实现相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |

## 7. 目录结构

```
csharp/
├── nevext/
│   └── Nevext.cs           # 库本体：单文件、零依赖，拷入任意工程即用
├── test/
│   ├── TestFramework.cs    # 零依赖自研框架（对齐 cpp/test 的设计取舍）
│   ├── TestConformance.cs  # 55 条通用语料 + dump
│   ├── TestImpl.cs         # C# 私有的 Impl 套件（5 条）
│   └── TestMain.cs         # 入口：--dump / 筛选 / 退出码
├── nevext.csproj           # 单一工程：库 + 测试编进一个可执行文件
├── test.cmd                # Windows 便捷入口（构建 + 直接跑 exe）
├── .gitignore              # bin/ obj/
└── docs/test-plan.md       # 本文档
```
