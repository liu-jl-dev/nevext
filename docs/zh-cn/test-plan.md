# Nevext 跨实现通用测试计划

> 适用范围：`cpp/nevext/`（C++20，header-only）与 `gdscript/nevext/`（Godot 4.x GDScript）
> 通用语料落地位置：`cpp/test/test_conformance.cpp`、`gdscript/test/test_conformance.gd`
> 各实现私有计划：C++ 见 `cpp/docs/test-plan.md`；GDScript 见 `gdscript/docs/test-plan.md`
> 最后更新：2026-10-17

## 1. 为什么需要这份文档

Nevext 有两套独立实现。两处的算法各自演化，很容易在以下地方悄悄分叉：

- 裸串的终止字符集（多一个字符、少一个字符，肉眼极难发现）；
- 转义在词法层还是语法层展开（决定原始串里 `\n` 的去向）；
- 错误是否被记录、报在第几行、是否会因为「按行去重」而被吞掉；
- `pauseWhenError` 开关在出错时保留多少语句。

本文件定义**一份与语言无关的通用语料**，两套实现各自用本机代码实现同一批用例、断言同一批期望值。
只要语料的 ID 与期望值不变，两套实现就不可能在上述分叉点上各自漂移而不被发现。

**角色划分**

| 角色 | 实现 | 说明 |
| --- | --- | --- |
| 参考实现 | `cpp/nevext/` | 行为语义以此为准。出现分歧时先看 C++，判定它是缺陷还是规范；修正后再同步到 GDScript 与本文件 |
| 对照实现 | `gdscript/nevext/` | 必须逐条复现参考实现的行为 |

## 2. 一致性契约

### 2.1 被测 API 面

两套实现必须都能按下述方式驱动（名字可随语言习惯调整，语义必须一致）：

| 环节 | C++ | GDScript |
| --- | --- | --- |
| 词法器构造 | `Lexer lexer(std::u16string&)` | `Nevext.Lexer.new(code: String)` |
| 取下一个 token | `lexer.NextToken() -> Token` | `lexer.next_token() -> Token` |
| token 字段 | `{line, type, value}` | `{line, type, literal}` |
| 词法错误容器 | `lexer.errors` | `lexer.errors` |
| 语法器构造 | `Parser parser(lexer)` | `Nevext.Parser.new(lexer)` |
| 解析入口 | `parser.ParseProgram(prog, pauseWhenError)` | `parser.parse_prog(pause_when_error)` |
| 产物 | `Statement{line, strings}` 数组 | `Statement{line, strings}` 数组 |
| 语法错误容器 | `parser.errors` | `parser.errors` |

返回值约定：成功为 `0`、失败为 `1`（C++ `Result::Success == 0`，GDScript `Result.OK == 0`）。

### 2.2 token 类型必须逐项对齐

`Unexpected, EndOfFile, RawStr, QuotedStr, Sep` —— 名称与顺序在两处完全一致（GDScript 的 `TokenType`
此前是 `Unexpected, EOF, Str, Sep`，已按此约定改写）。

### 2.3 错误按「语义种类」断言

错误消息的**措辞**是实现细节，**位置与性质**才是契约。所以所有跨实现断言都写成 `(行号, 种类)`：

| 种类 | 含义 | 触发场景 |
| --- | --- | --- |
| `UnclosedString` | 引号串到输入结束仍未闭合 | 词法层，报在**扫描结束**所在行 |
| `InvalidEscape` | 转义序列非法（转义表中没有该字符） | 语法层，报在**该引号串起始**所在行 |
| `UnexpectedToken` | 语法层读到了 `Unexpected` token | 语法层，报在**该 token 起始**所在行 |

各实现内部仍保留人类可读消息（两侧目前措辞一致），测试时用一个「消息 → 种类」的映射函数转换，
参考实现里形如 `cpp/test/test_util.hpp` 的 `ErrorKind()`。改措辞不需要改用例。

### 2.4 不可跨实现断言的东西

以下几项**不在**通用语料范围内，由各实现的私有套件覆盖：

- `Token::value` 的生命周期（C++ 是缓冲区视图，GDScript 是独立字符串）；
- 非 BMP 字符的**内部码元数**（C++ 内部是 UTF-16 代理对，GDScript 是 UTF-32 单码元）——
  通用语料只断言最终字符串**内容相等**，不断言长度；
- 性能：两套实现的字符串模型不同，数量级差异属正常（长输入用例只进各自的私有套件）。

## 3. 通用语料

### 3.1 记法

- 输入与期望值均用转义记法书写：`\n`=换行、`\r`=回车、`\t`=制表符、`\v`=垂直制表、`\b`=退格、`\f`=换页；
  `\\`=字面反斜杠；其余字符字面理解。
- 产物一列中，每条语句写作 `行号:["串1","串2",...]`，多条语句之间用一个空格分隔；`空` 表示零条语句。
- 错误一列写作 `(行号,种类)` 的有序序列，**顺序敏感**；`无` 表示 `lexer.errors` 与 `parser.errors` 都为空。
- `pause` 是解析入口的 `pauseWhenError` 开关。

### 3.2 A 组：程序骨架与语句切分

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| A01 | （空串） | true | OK | 空 | 无 |
| A02 | `abc` | true | OK | `1:["abc"]` | 无 |
| A03 | `a, b\tc` | true | OK | `1:["a","b","c"]` | 无 |
| A04 | `a b; c d; e` | true | OK | `1:["a","b"] 1:["c","d"] 1:["e"]` | 无 |
| A05 | `a; b` | true | OK | `1:["a"] 1:["b"]` | 无 |
| A06 | `;;;a;` | true | OK | `1:["a"]` | 无 |
| A07 | `a,,b` | true | OK | `1:["a","b"]` | 无 |
| A08 | `  \t\n ` | true | OK | 空 | 无 |
| A09 | `a b\nc d;` | true | OK | `1:["a","b","c","d"]` | 无 |
| A10 | `windowSize 1280, 720;` | true | OK | `1:["windowSize","1280","720"]` | 无 |

### 3.3 B 组：引号串

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| B01 | `"hello world" x` | true | OK | `1:["hello world","x"]` | 无 |
| B02 | `'abc'` | true | OK | `1:["abc"]` | 无 |
| B03 | `"" z` | true | OK | `1:["","z"]` | 无 |
| B04 | `'a' "b" c` | true | OK | `1:["a","b","c"]` | 无 |
| B05 | `'"' "'"` | true | OK | `1:["\"","'"]` | 无 |
| B06 | `"a"b` | true | OK | `1:["a","b"]` | 无 |

### 3.4 C 组：转义

转义只在**语法层**对 `QuotedStr` 展开；裸串一律不展开。

| ID | 输入 | pause | 返回值 | 产物 | 错误 | 说明 |
| --- | --- | --- | --- | --- | --- | --- |
| C01 | `"a\nb"` | true | OK | `1:["a\nb"]` | 无 | `\n` 展开为真实换行 |
| C02 | `"\n\r\t\v\b\f"` | true | OK | `1:["\n\r\t\v\b\f"]` | 无 | 6 个控制字符全部展开 |
| C03 | `"a\\"` | true | OK | `1:["a\\"]` | 无 | `\\` 展开为字面反斜杠 |
| C04 | `"ab\"cd"` | true | OK | `1:["ab\"cd"]` | 无 | 转义引号不再闭合字符串 |
| C05 | `'a\qb'` | true | Err | `1:["a"]` | `(1,InvalidEscape)` | 单引号串同样做转义展开 |
| C06 | `"a\qb"; c` | true | Err | `1:["a"]` | `(1,InvalidEscape)` | 语句保留已展开的前缀；遇错即停，后续 `c` 被丢弃 |
| C07 | `"a\qb"; c` | false | Err | `1:["a"] 1:["c"]` | `(1,InvalidEscape)` | 关闭暂停后继续解析，后续语句照产 |
| C08 | `"a\\\nb"` | true | Err | `1:["a"]` | `(1,InvalidEscape)` | 见第 6 节 X1：`\`+换行不在转义表内 |
| C09 | `"x\ny\tz"` | true | OK | `1:["x\ny\tz"]` | 无 | 混合转义 |

### 3.5 D 组：裸串边界

裸串的终止字符 = 空白 + 逗号 + `;` + `"` + `'`，且在 `//` / `/*` 处提前终止。

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| D01 | `a\\b` | true | OK | `1:["a\\b"]` | 无 |
| D02 | `a\\nb` | true | OK | `1:["a\\nb"]` | 无 |
| D03 | `a\\qb` | true | OK | `1:["a\\qb"]` | 无 |
| D04 | `a"b` | true | Err | `1:["a"]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| D05 | `a"b` | false | Err | `1:["a"] 1:["b"]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| D06 | `a'b` | false | Err | `1:["a"] 1:["b"]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| D07 | `a//comment` | true | OK | `1:["a"]` | 无 |
| D08 | `a/**/ b` | true | OK | `1:["a","b"]` | 无 |
| D09 | `"a//b"` | true | OK | `1:["a//b"]` | 无 |
| D10 | `a\bb` | false | OK | `1:["a\bb"]` | 无 |

> D10 是防止两侧「raw_str_end_chars 集合不一致」回归的关键用例：退格**不是**终止字符，整串是一个裸串。

### 3.6 E 组：注释

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| E01 | `// only comment` | true | OK | 空 | 无 |
| E02 | `/* c1\nc2 */ d` | true | OK | `2:["d"]` | 无 |
| E03 | `a /* xx` | true | OK | `1:["a"]` | 无 |
| E04 | `a /* x\nb; c;` | true | OK | `1:["a"]` | 无 |
| E05 | `/**/a` | true | OK | `1:["a"]` | 无 |
| E06 | `a //c1\n/* c2 */ b` | true | OK | `1:["a","b"]` | 无 |

> E03 / E04 固化「未闭合的多行注释不是错误」这条语法规定：注释延伸到输入结束，其后内容全部丢弃，
> **两个错误容器都必须为空、返回 OK**。

### 3.7 F 组：行号

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| F01 | `a;\nb;\nc;` | true | OK | `1:["a"] 2:["b"] 3:["c"]` | 无 |
| F02 | `a;\r\nb;` | true | OK | `1:["a"] 2:["b"]` | 无 |
| F03 | `"a\nb"; c;` | true | OK | `1:["a\nb"] 2:["c"]` | 无 |
| F04 | `"a\\\nb"; c;` | true | Err | `1:["a"]` | `(1,InvalidEscape)` |
| F05 | `a "b\nc` | true | Err | `1:["a"]` | `(2,UnclosedString) (1,UnexpectedToken)` |

> F03 / F05 覆盖「引号串内部换行计入行号」（原缺陷 D3）。F05 尤其重要：词法错误报在串**结束**行，
> 语法错误报在串**起始**行，两者行号不同，忘了一半就会露馅。

### 3.8 G 组：错误与错误恢复

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| G01 | `a "bc` | true | Err | `1:["a"]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| G02 | `a "bc` | false | Err | `1:["a"] 1:["bc"]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| G03 | `"a\qb" "c\qd"; e` | false | Err | `1:["a","c"] 1:["e"]` | `(1,InvalidEscape) (1,InvalidEscape)` |
| G04 | `"abc` | true | Err | 空 | `(1,UnclosedString) (1,UnexpectedToken)` |
| G05 | `"a\nb` | true | Err | 空 | `(2,UnclosedString) (1,UnexpectedToken)` |

> G03 固化「同一行的多处错误都要记录」（原缺陷 D4，去重逻辑已移除）。
> G01 / G02 说明未闭合失败时**不吞掉剩余输入**：关闭暂停后 `bc` 会被当作裸串读出来。

### 3.9 H 组：编码与其它

| ID | 输入 | pause | 返回值 | 产物 | 错误 |
| --- | --- | --- | --- | --- | --- |
| H01 | `a;"b";c` | true | OK | `1:["a"] 1:["b"] 1:["c"]` | 无 |
| H02 | `"😀" x`（U+1F600） | true | OK | `1:["😀","x"]` | 无 |
| H03 | `"` | true | Err | 空 | `(1,UnclosedString) (1,UnexpectedToken)` |
| H04 | `海上生明月 天涯共此时` | true | OK | `1:["海上生明月","天涯共此时"]` | 无 |

> H02 覆盖非 BMP 字符：C++ 内部走 UTF-16 代理对，GDScript 内部是 UTF-32 单码元，
> 但双方产出的字符串**内容必须逐字节相等**。为保证源文件不受编码影响，两侧都用运行时构造 / 通用字符名
> 转义（C++ `\U0001F600`，GDScript `char(0x1F600)`）来生成输入。
> H03 保证单个引号既不崩溃也不死循环。

## 4. 需求 → 用例追溯矩阵

| 规范条款（docs/zh-cn/overview.md） | 对应用例 |
| --- | --- |
| `prog : stmt*`（程序为语句列表） | A01、A02、A04、A05 |
| 逗号等同空白 | A03、A07、A10、B04 |
| 连续/多余逗号不产生空串条目 | A07 |
| 空语句不保留 | A06 |
| 最后一个语句可省略分号 | A05、A09、D08 |
| `raw_str` 规则（遇空白/逗号/分号/引号终止） | D01~D06、D10 |
| `raw_str` 遇注释提前终止 | D07、D08 |
| `raw_str` 不转义 | D01、D02、D03 |
| `sq_str` / `dq_str` | B01、B02、B04、B05、B06 |
| 异种引号无需转义 | B05 |
| 引号串内容可含空白与注释记号 | B01、D09 |
| 引号串可跨行，结果含换行；串内换行计入行号 | F03、F05 |
| 转义表 `\\ \' \" \n \r \t \v \b \f` | C01、C02、C03、C04、C09 |
| 非法转义：报错并保留已展开前缀 | C05、C06、C07 |
| 单引号串同样展开转义 | C05 |
| 单行注释 `//` | E01、D07、E06 |
| 多行注释 `/* */` | E02、E05、D08、E06 |
| 未闭合多行注释不是错误 | E03、E04 |
| 未闭合引号串是错误 | D04、D05、D06、G01~G05、H03 |
| `pauseWhenError` 两种取值 | C06/C07、D04/D05、G01/G02 |
| 语句行号 = 首 token 所在行 | F01、F02、F03、E02、E06 |
| 非 ASCII / 非 BMP 内容原样保留 | H02、H04 |
| 词法错误报在串结束行、语法错误报在串起始行 | F05、G05 |

## 5. 落地位置与运行方式

| 实现 | 通用语料 | 运行入口 | 私有计划 |
| --- | --- | --- | --- |
| C++ | `cpp/test/test_conformance.cpp`（套件名 `Conformance`） | `ctest --test-dir build -C Debug` 或 `./build/test/nevext_test Conformance` | `cpp/docs/test-plan.md` |
| GDScript | `gdscript/test/test_conformance.gd`（套件名 `Conformance`） | `godot --headless --path gdscript --script res://test/test_main.gd` | `gdscript/docs/test-plan.md` |

两侧用例名统一为 `Conformance.A01` … `Conformance.H04`，与第 3 节的 ID 一一对应，
便于 `Conformance.C0` 这样按前缀筛选用例。

**跨实现对照**：两套测试都支持 dump 模式，按同一行格式打印全部语料的实际结果：

```
<ID>|<OK|Err>|<program>|<errors>|pause=<0|1>
```

其中 `<program>` 为 `<行号>:["<串>",...]` 用 `|` 连接，`<errors>` 为 `<行号>:<种类>` 用 `,` 连接，
字符串先做转义再拼接：`\\` `\n` `\r` `\t` `\v` `\b` `\f` `\xNN`，并把 `"` 转义为 `\"`
（否则 B05 这种含引号的结果会让整行不可解析）。转义顺序必须是「先反斜杠、后引号」。

```bash
# 两侧各跑一次 dump，再逐行 diff —— 只要实现分叉就会显形。
# Windows 上 C++ 侧的 stdout 走文本模式会把 \n 翻译成 CRLF，Godot 侧是 LF，
# 所以 diff 要加 --strip-trailing-cr，否则每一行都会被判为不同。
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(godot --headless --path gdscript --script res://test/test_main.gd -- --dump)
```

> 注意 Godot 的参数分隔：`--script` 之后再用一个单独的 `--`，其右侧才是脚本参数
> （`OS.get_cmdline_user_args()`），否则 `--dump` 会被 Godot 自己当作引擎参数吃掉。

## 6. 已知缺口与不在范围内的项

| 编号 | 现象 | 状态 | 处置 |
| --- | --- | --- | --- |
| **X1** | 引号串里写 `\`+换行（常见于「续行」写法）时，词法层把它当普通物理换行计入行号，但转义表里没有 `\<换行>`，语法层最终报 `InvalidEscape` 并只保留前缀 | 已确认为**规范缺口**（两套实现行为一致） | 由 C08 / F04 固化为当前行为：返回 `Err`、产物只剩前缀、`(1,InvalidEscape)`。若将来要把行续接纳入语法，需同时改 props 两侧实现与本表 |
| **X2** | 非 BMP 字符的内部码元数不同（UTF-16 代理对 vs UTF-32） | 已确认为实现差异，不影响契约 | 通用语料只断言字符串内容相等（H02），不断言长度/码元数 |
| **X3** | GDScript 的字符串模型导致长输入比 C++ 慢约三个数量级 | 已确认为实现差异 | 长输入（10k 裸串、2k 语句、全前缀扫描）只进各实现的私有套件，不进通用语料 |

## 7. 维护规则

1. **新增用例先改本文档。** 先在 `docs/zh-cn/test-plan.md` 第 3 节加一行（含 ID、期望值、覆盖点），
   再同步到 `cpp/test/test_conformance.cpp` 与 `gdscript/test/test_conformance.gd`。
2. **ID 不复用。** 用例改名可以，ID 一旦分配就固定，方便跨版本对照。
3. **期望值来自参考实现，但必须人工复核。** 用 C++ 跑出结果后再逐条对着 `overview.md` 过一遍，
   不要把实现缺陷固化成期望值；判断为缺陷的走各自的私有计划处置。
4. **分歧修正必须双侧落地。** 任何跨实现的分叉修正，都要在两侧同时补回归用例，
   并在本文档第 6 节或各自的私有计划里登记。
