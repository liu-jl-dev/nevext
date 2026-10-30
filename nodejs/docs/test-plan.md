# Nevext JavaScript 实现测试计划

> 适用范围：`nodejs/nevext/nevext.js`（Node.js，ESM 单文件，零第三方依赖）
> 测试代码目录：`nodejs/test/`
> **跨实现通用语料不在本文档**：五套实现共用的 55 条语料见 `docs/zh-cn/test-plan.md`，
> 落地在 `nodejs/test/test_conformance.js`。本文档只描述 JS 私有的 `Impl` 套件。
> 文档最后更新：2026-10-30（初版，60 用例 / 449 断言通过）

## 1. 目标与非目标

**目标**

1. 用与 C++ / GDScript 相同的口径守住 JS 实现的行为：`Conformance.*` 55 条语料 +
   dump 门禁（`node nodejs/test/test_main.js --dump` 与 C++ 侧输出必须逐字节一致）。
2. 覆盖 JS 侧的工程性关注点：对象复用（reset）、终止性、大输入、码元保真。
3. 保持零依赖：`node` 一条命令即可运行，`package.json` 不引入任何依赖。

**非目标**

- 不测浏览器环境（`nodejs/` 目前只面向 Node；实现本体是纯 JS，理论上可直接内嵌，
  但不在本计划的验证范围内）。
- 不做性能基准：JS 与 C++ 的性能差异属实现差异（见 `docs/zh-cn/test-plan.md` X3 的同款口径）。

## 2. 被测对象与 API 形状

单文件 `nevext/nevext.js`（ESM，`package.json` 声明 `"type": "module"`），命名导出：

| 导出项 | 说明 |
| --- | --- |
| `TokenType` | 5 个枚举项，取值顺序与 C++ 一致（`Unexpected`=0 … `Sep`=4） |
| `Result` | `Success`=0 / `Error`=1，可直接写 `if (res.err)` |
| `Lexer` | `new Lexer(input)`、`nextToken()`、`reset(input)`；`errors: string[]` |
| `Parser` | `new Parser(lexer)`、`parseProgram(pauseWhenError=true)`、`reset(lexer)`；`errors: string[]` |
| `Statement` / `ParseResult` / `Token` | 产物类型 |
| `expandEscapes` | 转义展开（暴露出来仅供测试与工具使用） |

与 C++ 的差异：`parseProgram` 走**返回值**（`ParseResult { err, val }`），不用 C++ 的出参；
方法名按 JS 惯例 camelCase（`nextToken` / `parseProgram`）。除此之外的行为契约与 C++ 逐条一致。

## 3. 实现要点（JS 特有）

1. **字符串即码元。** JS 字符串天然是 UTF-16 码元序列，与 C++ 的 `std::u16string` 同构：
   代理对、孤立代理项都按码元原样保留（`Impl.LoneSurrogatePreserved` 守护；GDScript 的
   UTF-32 模型无法表达孤立代理项，所以这条只在 JS 侧存在）。源文件统一 UTF-8，
   非 ASCII 语料一律写 `\uXXXX` 码元，与文档约定一致。
2. **游标代替切片。** C++ 用 `string_view` 前移，GDScript 每步 `substr()`（超线性）；
   JS 用「完整字符串 + `pos` 游标」，行为一致且整体 O(n)。`reset` 同时复位游标。
3. **错误消息逐字对齐 C++**（含句号后的空格），一致性断言通过「消息 → (行号, 种类)」
   映射比较，与另两套相同。
4. **dump 的 `show()` 必须与 C++ 的 `EscapeVisible` 逐项一致**：六个具名控制字符
   （`\n` `\r` `\t` `\v` `\b` `\f`）、反斜杠翻倍、其余控制字符 `\xNN`；引号在
   show 之后再转义（「先反斜杠、后引号」）。第一版就漏了这两类，靠 dump 门禁抓出——
   **改动 dump 相关代码后必须重跑门禁**。

## 4. 运行方式

```bash
# 全部用例（Conformance 55 条 + Impl 5 条）
node nodejs/test/test_main.js          # 或 nodejs\test.cmd / npm test --prefix nodejs

# 按名字子串筛选
node nodejs/test/test_main.js Conformance.C0

# dump 门禁（与 C++ 逐字节 diff）
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(node nodejs/test/test_main.js --dump)
```

**当前基线（Node v22）**：60 个用例 / 449 条断言全部通过（Conformance 55 + Impl 5）。

## 5. 用例清单：`Impl` 套件（5 个，test_impl.js）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| JS-01 | `ResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句、`lexer.line` 回到 1、两侧错误容器为空（对应 GDScript IMP-02、C++ LEX-26 / PAR-18） |
| JS-02 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile`（不死循环、不爆栈） |
| JS-03 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字符 |
| JS-04 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首末语句内容与末行号（1 / 500）正确 |
| JS-05 | `LoneSurrogatePreserved` | `x <孤立高代理> y "<孤立低代理>"` | 孤立代理项按码元原样保留，无错误（JS 特有） |

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| — | 与 C++ / GDScript 相同的规范缺口（如 X1 续行转义）记录在 `docs/zh-cn/test-plan.md` 第 6 节，此处不重复 | — | — |
| ~~J1~~ | 原为「未声明 `"type"`，ESM 只能走默认互操作」；2026-10-30 已改为 ESM（`"type": "module"` + 命名导出），全量测试与 dump 门禁通过后关闭 | 已关闭 | — |
