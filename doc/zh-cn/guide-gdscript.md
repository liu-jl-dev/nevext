# Nevext GDScript 使用指南

本指南介绍 GDScript 实现的集成方式与 API 用法。关于语言本身的语法规则，请先阅读
[概述](./overview.md)。

> ## ⚠ 实现状态：目前并不符合 Nevext 标准
>
> GDScript 实现是一个**未完成的移植**，与[概述](./overview.md)定义的语法规范、
> 与 [C++ 实现](./guide-cpp.md) 之间都存在偏差。已确认的主要问题：
>
> 1. **转义会静默丢字符** —— `"a\nb"` 解析成 `a` + 换行，`b` 丢失且不报错（第 8.2 节）；
> 2. **raw 字符串边界与 C++ 不同** —— `abc//comment` 会被整体当成一个字符串；
>
本文档中的 GDScript 行为均在 **Godot 4.7.2 stable (mono)** 上实机运行验证。

## 集成步骤

Nevext 的 GDScript 实现只有一个文件，`class_name Nevext` 会注册为全局类名，
不需要 `preload`。

1. 把 `gdscript/nevext/nevext.gd` 复制到你的工程中（例如 `res://nevext/nevext.gd`）；
2. 等待 Godot 扫描文件系统，或执行「项目 → 工具 → 重新扫描文件系统」；
3. 直接使用 `Nevext.Lexer`、`Nevext.Parser` 等。

## 环境要求

- Godot 版本：**Godot 4.x**。实测环境为 4.7.2 stable (mono)，`project.godot` 声明 features 为 `4.7`
- 无第三方依赖，只用引擎内置类型
- 全局类名 `Nevext` 不能在同一工程内重复注册，同一工程只能放一份 `nevext.gd`

## 示例项目

仓库中包含一个 Godot 示例工程。[程序入口：gdscript/test/test_01.gd](../../gdscript/test/test_01.gd)

### 目录结构

```
gdscript/
├── project.godot          # 工程配置（工程名 nevext-demo）
├── icon.svg               # 工程图标
├── nevext/
│   └── nevext.gd          # ★ 库本体，只需拷贝这个文件
└── test/
    ├── test_01.gd         # 可运行示例
    └── test_01.tscn
```

**运行注意事项：** `project.godot` 中**没有**设置 `run/main_scene`，
直接 `--path` 启动会报 `no main scene defined`，必须显式指定场景：

```bash
godot --headless --path gdscript res://test/test_01.tscn
```

## 快速上手

下面是完整可运行的示例（取自 `gdscript/test/test_01.gd`）：

```gdscript
extends Control

func _ready() -> void:
	main01()
	get_tree().quit()

func main01():
	# GDScript 多行字符串字面量；其中的 \" 会先被 GDScript 转义，
	# 因此交给 Nevext 的内容里不含反斜杠
	var src := '
	// 单行注释
    hello \"world\";
    /*
    多行注释  
    */
    海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾
	'
	var lexer := Nevext.Lexer.new(src)
	var parser := Nevext.Parser.new(lexer)
	var res:Nevext.ParseResult = parser.parse_prog()
	if res.err:
		print("==== Parser errors: ================================")
		for e in parser.errors:
			print(e)

	for i in range(len(res.val)):
		var s := res.val[i]
		print("statement ", i + 1, ' at line ', s.line, ':')
		for st in s.strings:
			print('  ', st)
```

实际输出：

```
statement 1 at line 3:
  hello
  world
statement 2 at line 7:
  海上生明月
  天涯共此时
```

## API 参考

所有类型都是 `Nevext` 类的内部类，通过 `Nevext.XXX` 访问。

### 枚举

```gdscript
enum TokenType {
	Unexpected, EOF, Str, Sep,
}

class Result:
	var err:int = OK     # 0 = OK，1 = Err
	var val              # 携带的值
	const OK  = 0
	const Err = 1
```

> **要点：** 只有 `OK` / `Err` 两种结果，没有 C++ 的 `UnknownError`、
> `UnclosedStringError` 等细分类别，无法从返回值区分错误类型。

### Token 与 Lexer

```gdscript
class Token:
	var type:TokenType
	var line:int
	var literal:String      # 已完成转义的字符串内容

class Lexer:
	var escape_func:Callable = Nevext.defalt_escape   # 可替换的转义函数
	var code:String
	var line:int = 1

	func _init(code_:String="") -> void
	func reset(code_:String)
	func next_token() -> Token
```

一般不需要直接使用 `Lexer`，交给 `Parser` 驱动即可。

`Lexer` 持有的是 `String`（值拷贝），**不存在** C++ 版本中「源字符串必须活得更久」
的约束；转义也在词法阶段完成，因此 `TokenType` 无需区分 `RawStr` 与 `QuotedStr`。

### Statement、ParseResult 与 Parser

```gdscript
class Statement:
	var line:int
	var strings:Array[String]
	func _to_string() -> String

class ParseResult:
	var err:int
	var val:Array[Statement]

class Parser:
	var lexer:Lexer
	var errors:Array[String]

	func _init(lexer_:Lexer=null) -> void
	func reset(lexer_:Lexer)
	func parse_prog(pause_when_error:bool=true) -> ParseResult
```

**`parse_prog` 参数**

| 参数 | 说明 |
| --- | --- |
| `pause_when_error` | `true`（默认）：遇到第一个错误立即停止解析；`false`：继续解析，尽量多收集错误 |

无论哪种模式，出错前已经解析出的语句都会保留在 `res.val` 中。

### 自定义转义（C++ 版没有的能力）

`Lexer.escape_func` 是 `Callable`，默认使用 `Nevext.defalt_escape`。
设为 `Nevext.null_escape` 可完全关闭转义：

```gdscript
var lexer := Nevext.Lexer.new(src)
lexer.escape_func = Nevext.null_escape   # 原样返回，不做转义
```

实测：`null_escape` 下 `"a\nb"` 得到字面量 `a\nb`（反斜杠保留），
默认函数则得到 `a` + 换行（`b` 丢失，见第 8.2 节）。

也可以传入自定义函数，签名需为 `func(s:String) -> Nevext.Result`：
成功时置 `res.val`，失败时置 `res.err = Nevext.Result.Err`。

## 错误处理

### 错误容器只有一个

与 C++ 版不同，GDScript 版的 `Lexer` **没有** `errors` 数组，所有错误都汇总到
`Parser.errors`。

好处是不存在 C++ 版那种「漏看 `lexer.errors`」的坑；代价是丢失词法阶段的细节——
未闭合字符串只体现为一条 `exception: line N: < unclosed string >`。

### 错误没有去重

两版一致：**都不做去重**。C++ 版的 `Parser::error()` 曾有行号去重（同一行只报第一条），
现已移除；GDScript 版从未有过这个逻辑。
实测同一行制造两个错误（`pause_when_error = false`）时，两条都进入 `errors`：

```json
["exception: line 1: < unexpected escape char >",
 "exception: line 1: < unclosed string >"]
```

错误消息格式是 `'exception: line ' + str(line) + ': ' + token 字面量`，
与 C++ 的 `line N: unexpected token.` 措辞不同，两边统一时需同步修改。

### 实测行为

| 输入 | `res.err` | 保留的语句 | `errors` |
| --- | --- | --- | --- |
| `a; b; c`（末句无分号） | 0 | 3 条 | 无 |
| `;;;a;`（空语句） | 0 | 1 条 | 无（空语句被丢弃） |
| `abc "unclosed` | 1 | `[["abc"]]` | `exception: line 1: < unclosed string >` |
| `abc "bad\q"` | 1 | `[["abc"]]` | `exception: line 1: < unexpected escape char >` |

`errors` **不会自动清空**，只有 `reset()` 会清空。实测：替换 `parser.lexer` 后
不调用 `reset()` 再解析，上一次的错误仍留在数组里。复用 `Parser` 前请调用
`parser.reset(new_lexer)`。

## 字符串编码

### Godot 的 String 无需转码

Godot 的 `String` 内部以 UTF-32 存储，可以直接处理中文与 emoji，
**不需要** C++ 版那样的 UTF-16 ↔ UTF-8 转换，解析结果可直接 `print`。

这与 C++ 版形成对照：C++ 版内部使用 `std::u16string`，打印前需要用
`demo/utf.hpp` 转成 UTF-8，且 emoji 会占 2 个 `char16_t`。

### 控制台输出

Godot 的输出面板与 `print()` 均为 UTF-8，无需额外设置。
这与 C++ 版不同——C++ 在 Windows 控制台需要先调用
`SetConsoleOutputCP(CP_UTF8)`，否则中文乱码。

## 已知偏差与缺陷

### 行为差异对照（均已实测）

| 输入 | GDScript 实测 | C++ 实测 |
| --- | --- | --- |
| `"a\nb"` | `a` + 换行（**丢了 `b`**），不报错 | `a` + 换行 + `b` |
| `"ab\"cd"` | `ab"`（**丢了 `cd`**） | `ab"cd` |
| `"\nx"` | 换行 + `x` | 换行 + `x`（一致） |
| `abc//comment` | 一个字符串 `abc//comment` | 字符串 `abc`，其余视为注释 |
| `a"b` | 无错误，一个字符串 `a"b` | **报错**，解析为 `a` 与 `b` 两条语句 |
| `x //tail`（末尾无换行） | `x` | `x`（一致） |

**差异根源：raw 字符串的中断条件不同**

| 中断条件 | C++ | GDScript |
| --- | --- | --- |
| 空白字符（空格、逗号、制表等） | ✅ 中断 | ✅ 中断 |
| `;` | ✅ 中断 | ✅ 中断 |
| `"` 与 `'` | ✅ 中断 | ❌ 不中断 |
| `//` 或 `/*` | ✅ 中断 | ❌ 不中断 |

### 转义会静默截断后续内容

`defalt_escape` 处理完一个转义字符后，重新截取剩余串时用了错误的下标：

```gdscript
res.val += s.substr(0, pos)
s = s.substr(pos)          # s 已重新以 '\' 开头
...
s = s.substr(pos + 2)      # ← 应为 s.substr(2)；pos 是"重新定位前"的下标
```

转义字符**不在字符串开头**（`pos > 0`）时会多跳过 `pos` 个字符，
其后内容被**静默丢弃且不报错**。实测：

| 输入（引号内原文） | 结果 | 期望 |
| --- | --- | --- |
| `a\nb` | `a` + 换行 | `a` + 换行 + `b` |
| `x\ny\tz` | `x` + 换行 + 制表 + `z` | `x` + 换行 + `y` + 制表 + `z` |
| `ab\"cd` | `ab"` | `ab"cd` |
| `\n` | 换行 | 换行（转义在开头，`pos == 0`，偶然正确） |

**修复：** 把 `s = s.substr(pos + 2)` 改为 `s = s.substr(2)`。
该修复已在临时副本上实测通过，改后结果与 C++ 完全一致。

### 性能随规模急剧劣化

`Lexer` 每推进一个字符就做一次 `code = code.substr(1)`，是 O(n²) 量级。
实测（headless，同一份输入，仅改变规模）：

| 输入字符数 | 解析耗时 |
| --- | --- |
| 1.7 万 | 33 ms |
| 7.1 万 | 371 ms |
| 30.9 万 | **23,415 ms（23 秒）** |

字符数增长约 4 倍时耗时增长 10~60 倍，比二次方更陡。
KB 级以上的输入请务必评估，或改用 C++ 实现。

### 注释跳过依赖隐式语义（当前正确，建议改写）

`_skip_ws` 中跳过单行注释的写法：

```gdscript
elif _test('//'): code = code.substr(code.find('\n'))
```

`find()` 找不到换行符时返回 `-1`。实测 `"//tail".substr(-1)` 在 Godot 中返回
**空串**（不是最后一个字符），因此末尾没有换行的单行注释**能被正确跳过**——
`x //tail` → `["x"]`、`//only` → 无语句，均与 C++ 一致。

这里**当前不是 bug**，但正确性依赖 `substr()` 对负下标的隐式处理，意图不明。
建议后续改为显式判断：

```gdscript
elif _test('//'):
	var nl := code.find('\n')
	code = '' if nl < 0 else code.substr(nl)
```

### 其他

| 问题 | 说明 |
| --- | --- |
| `Result` 无错误细分 | 见「枚举」一节 |
| 无单元测试 | demo 只有一段手工 `print`，没有断言；C++ 版同样缺失 |
| `config/name` 仍是 `nevext-demo` | 工程配置沿用旧名，不影响功能 |

## 生命周期与常见陷阱

| 陷阱 | 说明 |
| --- | --- |
| **类名冲突** | `class_name Nevext` 不能在同一工程注册两次 |
| **工程无主场景** | 未设 `run/main_scene`，运行时必须显式传场景路径 |
| **未扫描会报"Nevext 未声明"** | 新拷入脚本后需让 Godot 重新扫描文件系统，否则全局类未注册 |
| **`errors` 需手动清** | 只有 `reset()` 会清空，见「错误处理」一节 |
| **GDScript 字面量会先转义一次** | 源码写 `\"world\"`，交给 Nevext 时已是 `"world"`；想测 Nevext 层转义需写 `\\n` |
| **大文件性能** | 见上一节，30 万字符约 23 秒 |
| **逗号等同空格** | `a, b` 与 `a b` 解析结果相同，这是语法规定而非 bug |

## 许可

本项目使用 MIT 协议，详见仓库根目录的 [LICENSE](../../LICENSE)。
