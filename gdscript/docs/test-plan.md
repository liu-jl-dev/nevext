# Nevext GDScript 实现测试计划

> 适用范围：`gdscript/nevext/nevext.gd`（Godot 4.x）
> 测试代码目录：`gdscript/test/`
> **跨实现通用语料不在本文档**：C++ 与 GDScript 共用的 55 条语料（ID `A01`~`H04`）
> 定义在 `docs/zh-cn/test-plan.md`，落在 `gdscript/test/test_conformance.gd`。
> 本文档只描述 GDScript 私有的 `Impl` 套件，以及跑 Godot 测试时特有的坑。
> 最后更新：2026-10-17

## 1. 目标与非目标

**目标**

1. 守住「GDScript 实现必须复现 C++ 参考实现的行为」这条主线（靠通用语料，非本文档职责）。
2. 覆盖 GDScript 实现**独有**的面：可替换的 `escape_func` 扩展点、实例复用、
   递归/循环终止性，以及在 Godot 字符串模型下的长输入表现。
3. 给出一条命令就能跑、失败就有非零退出码的入口，接得上 CI。

**非目标**

- 不重新定义通用语料，那是 `docs/zh-cn/test-plan.md` 的职责。
- 不追求与 C++ 侧的性能可比：两者字符串模型不同，数量级差异属正常（已知缺口 X3）。
- 不引入 GDUnit / GUT 这类第三方测试框架——库本体零依赖，测试也应零依赖。

## 2. 被测对象

| 文件 | 职责 | 主要测试面 |
| --- | --- | --- |
| `gdscript/nevext/nevext.gd` | 全部：枚举、`Token`、`Lexer`、`Parser`、`Statement` | 通用语料 + `Impl` 套件 |

它是单文件实现，没有拆分，因此不做「按组件分文件」的测试切分；用例按**主题**分组。

## 3. 测试策略

1. **零依赖自研框架。** `test/test_framework.gd` 与 C++ 侧的 `cpp/test/test_framework.hpp`
   口径对齐：按名字子串筛选、失败不中断、最后打印汇总并以非零码退出。
2. **两套套件分工。**
   - `Conformance.*`：通用语料，断言口径是「返回值 + 语句列表 + `(行号, 错误种类)` 序列」。
     「错误种类」是对消息文本做的一次映射（`kind_of`），改措辞不影响用例。
   - `Impl.*`：只测 C++ 侧没有的东西，见第 5 节。
3. **断言失败信息要能自我定位。** GDScript 拿不到调用点的文件名与行号，
   所以每条断言都带中文说明（`"语句[0]字符串数"` 之类），而不是靠行号去找。
4. **长输入克制。** 见 X3：GDScript 每推进一个字符做一次 `substr()`，超线性；
   所以私有套件里的长输入用 1000 字符 / 500 语句这种量级，不追求和 C++ 同等规模。

## 4. 目录结构与运行方式

```
gdscript/
├── project.godot
├── test.cmd                 # Windows 便捷入口（可用 GODOT 环境变量指定 exe）
└── test/
    ├── test_main.gd         # 入口：解析参数 → register → run_all → quit(code)
    ├── test_framework.gd    # 断言 / 注册表 / 执行器
    ├── test_conformance.gd  # 跨实现通用语料（Conformance.A01 ~ H04）
    └── test_impl.gd         # GDScript 私有用例（Impl.*）
```

**运行**

```bash
# 全部用例
godot --headless --path gdscript --script res://test/test_main.gd

# 按名字筛选
godot --headless --path gdscript --script res://test/test_main.gd -- Conformance.C0
godot --headless --path gdscript --script res://test/test_main.gd -- Impl

# dump：打印语料实际结果
godot --headless --path gdscript --script res://test/test_main.gd -- --dump

# Windows
gdscript\test.cmd Conformance
```

参数里那个单独的 `--` 不能省：其右侧才是脚本参数（`OS.get_cmdline_user_args()`），
否则会被 Godot 当作引擎参数吃掉。

**与 C++ 侧对拍**（最省事的一致性门禁）：

```bash
diff --strip-trailing-cr \
     <(./build/test/nevext_test --dump) \
     <(godot --headless --path gdscript --script res://test/test_main.gd -- --dump)
```

## 5. 用例清单：`Impl` 套件（5 个，test_impl.gd）

| ID | 用例名 | 输入要点 | 期望要点 |
| --- | --- | --- | --- |
| IMP-01 | `EscapeFuncCanBeReplaced` | `escape_func = Nevext.null_escape` 解析 `"a\nb" x` | 成功；字符串保持字面 `a\nb`（`\` 与 `n` 都在）；无错误 |
| IMP-02 | `LexerAndParserResetAreReusable` | 同一 `Lexer` / `Parser` 实例，`reset` 后解析第二份源码 | 两次结果互不干扰；第二次 2 条语句且 `lexer.line` 回到 1 |
| IMP-03 | `PathologicalInputsTerminate` | 病态串 `a "b\c' /* xx //<换行>y ;<tab>'z"*/ ;` 的全部前缀 | 每个前缀都能在 200 步内扫到 `EndOfFile`（不死循环、不爆栈） |
| IMP-04 | `LongRawTokenIsComplete` | 1000 个 `x` + `;` | 单个 `RawStr` 完整保留 1000 字符 |
| IMP-05 | `ManyStatementsAreAllParsed` | 500 行 `sN a b;` | 500 条语句，首行性与末行号（1 / 500）及内容正确 |

> IMP-01 针对的是 C++ 实现**没有**的扩展点，所以不能进通用语料。
> IMP-03 是 C++ 侧 `EveryPrefixTerminates` 的 GDScript 对口版本，
> 规模取小值是因为 X3 的性能代价。

## 6. Godot 特有踩坑清单

写 GDScript 测试时撞过的坑，改测试前请先读一遍，能省不少时间：

| 坑 | 现象 | 处置 |
| --- | --- | --- |
| 删了 `.godot/` | `Parse Error: Identifier "Nevext" not declared in the current scope` | 全局类注册信息在 `.godot/global_script_class_cache.cfg`。不要删；新检出先 `--headless --editor --quit` 扫一次 |
| `var x := load(...)` | 整个文件解析失败：`Cannot infer the type of "x" variable` | `load()` 返回无静态类型的 Variant，必须显式标注类型。用 `godot --check-only --script 文件` 才能看到这条报错 |
| 套件实例只放局部变量 | `Attempt to call function 'null::check_case (Callable)' on a null instance` | `Callable.bind` 不增加引用计数，`register()` 返回后 RefCounted 被释放。实例要挂在 `static var` 上 |
| 内部类访问外层成员 | 行为不可靠 | `test_framework.gd` 因此不用内部类，改成「静态函数 + 显式传 `ctx` 字典」 |
| `const` 里调内置函数 | 解析失败 | 语料表用了 `char(0x08)` 构造不可见字符，所以写成 `static func cases()` 而不是 `const` |
| 脚本参数少了 `--` | `--dump` / `-v` 被 Godot 自己吃掉 | `--script xxx.gd -- <参数>`，`OS.get_cmdline_user_args()` 读取 |
| Windows 行尾 | 与 C++ 侧 dump 逐行都被判不同 | C++ 的 stdout 在 Windows 走文本模式会把 `\n` 变 CRLF，Godot 输出 LF；diff 加 `--strip-trailing-cr` |
| mono 版 exe | `Invalid wrapper executable name` | 别用 `godot.exe`（那是 mono 包装器），用 `Godot_v4.7.2-stable_mono_win64.exe` |

## 7. 当前基线

Godot 4.7.2 stable (mono)，headless：

- `Conformance.*`：55 个用例 / 400 条断言通过
- `Impl.*`：5 个用例 / 46 条断言通过
- 合计 **60 个用例 / 446 条断言**，含引擎启动约 2.7 秒
- 与 C++ 侧 `--dump` 的输出逐字节一致（55 行）

## 8. 后续计划

1. **结构化模糊测试**：IMP-03 只保证「不死循环」，可升级为「随机串 → 要么成功、要么至少一条错误」，
   用来搜 C++ 侧 D1 那种静默失败。
2. **性能阈值**：把 IMP-04 / IMP-05 改成打印耗时并设上限，防止改动引入数量级退化。
3. **CI 接入**：同时跑 `ctest`（C++）与本套件（GDScript），并把两侧 `--dump` 的 diff 作为一致性门禁。
4. **`escape_func` 的更多形态**：目前只测了 `null_escape`，可补一个自定义失败用例，
   验证「保留已展开前缀 + 一条 `InvalidEscape`」这条契约在自定义函数下也成立。
