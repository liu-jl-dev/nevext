# Nevext GDScript 使用指南

本指南介绍 GDScript 实现的集成方式与 API 用法。关于语言本身的语法规则，请先阅读
[概述](./overview.md)。

> **实现状态：已与 C++ 参考实现对齐。**
> GDScript 实现以 [C++ 实现](./guide-cpp.md) 为行为基准，`TokenType` 枚举的名称与顺序、
> 词法/语法的错误种类与位置、行号统计、`pause_when_error` 语义均已逐条对齐，
> 并由同一份跨实现语料守卫：详见 [跨实现通用测试计划](./test-plan.md)。
> 两侧任一行为发生变化而另一侧没跟上，测试就会红。
>
> 本文档中的 GDScript 行为均在 **Godot 4.7.2 stable (mono)** 上实机运行验证。

## 集成步骤

Nevext 的 GDScript 实现只有一个文件，**不注册 `class_name` 全局类名**
（不占用工程的全局命名空间，不会与你工程里的同名类冲突），用 `preload` 引入：

1. 把 `gdscript/nevext/nevext.gd` 复制到你的工程任意目录（例如 `res://nevext/nevext.gd`）；
2. 在每个用到它的脚本顶部声明一次：
   `const Nevext := preload("res://<拷贝路径>/nevext.gd")`；
3. 之后照旧使用 `Nevext.Lexer`、`Nevext.Parser`、`Nevext.VERSION` 等。

> v0.3.0 及之前靠全局类名直接写 `Nevext.XXX`；升 v0.4.0 只需补一行
> 上述 `const` 声明，其余写法不变，见[更新日志](../../CHANGELOG.md)。

## 可选集成方式：Autoload 全局单例

Godot 里「注册全局 Node」的机制是 **Autoload 单例**（项目 → 项目设置 →
全局 → 自动加载）。官方文档明确：Autoload 只接受场景或**继承 `Node` 的脚本**。
Nevext 本体是 `RefCounted`，直接注册会被引擎拒绝（Godot 4.7.2 headless 实测）：

```
ERROR: Failed to instantiate an autoload, script 'res://nevext/nevext.gd' does not inherit from 'Node'.
```

需要的话，可以在**使用方工程内**写一层 `extends Node` 的薄包装再注册
（建议与库文件解耦，包装放在你自己的工程里）：

```gdscript
# nevext_service.gd —— 随 Autoload 注册，例如命名为 NevextSvc
extends Node
const Nevext := preload("res://<拷贝路径>/nevext.gd")
```

注册后全局任何脚本可用（Godot 4.7.2 headless 实测通过）：

```gdscript
var lexer  = NevextSvc.Nevext.Lexer.new(src)
var parser = NevextSvc.Nevext.Parser.new(lexer)
var res    = parser.parse_program()
```

> **注意**：经由 Autoload 节点访问的 `Nevext` 常量是 Variant，上面的变量
> 必须用 `var` 而不是 `:=`（类型推断会解析失败）。

**本库不推荐 Autoload**：

| 考量 | 说明 |
| --- | --- |
| 单例名仍是全局标识符 | Autoload 的名字同样占用工程全局命名空间、同样一份只能一个，与「不用全局类名」的动机相悖 |
| 引入工程配置依赖 | 要改 `project.godot`，破坏「拷一个文件进工程即可用」的定位 |
| 单例化没有收益 | 解析库无状态，每次解析应新建 `Lexer` / `Parser`（或调用方显式 `reset` 复用）；官方文档将 Autoload 定位为「跨场景持久信息 / 全局状态」（玩家数据、场景切换服务等），纯函数式解析器不属于这类场景 |

## 加载方式与官方推荐

Godot 官方文档（GDScript reference 的 Classes as resources / Registering
named classes，与 Singletons (Autoload)）给出的脚本引用机制有四种：

| 方式 | 官方定位 | 用于本库 |
| --- | --- | --- |
| `const Nevext := preload("res://...")` | **官方标准做法**：脚本文件即 GDScript 资源，跨脚本使用必须 load / preload；preload 在编译期只解析一次，官方示例即 `const MyClass = preload("myclass.gd")` | ✅ 推荐（本仓库 demo / test 均此写法） |
| `class_name` 全局类 | 把脚本注册为编辑器全局类型，全工程免 preload | 可选：可在使用方自己的包装脚本上注册；库本体 v0.4.0 起不再注册 |
| Autoload 单例 | 常驻 root 的 Node 单例，用于跨场景持久状态；仅接受 `Node` 派生脚本 | 本体不可注册；`extends Node` 包装可注册但不推荐，见上一节 |
| `load()` 运行时加载 | 运行时按路径取资源，返回 Variant | 不推荐：失去静态类型与编译期检查（类型推断坑见 [GDScript 测试计划](../../gdscript/docs/test-plan.md) 第 6 节） |

官方没有单独的「GDScript 库发布指南」；对 Nevext 这类无状态、纯逻辑、
拷走即用的单文件库，`const + preload` 就是官方文档给出并被社区广泛
使用的默认集成方式。

## 环境要求

- Godot 版本：**Godot 4.x**。实测环境为 4.7.2 stable (mono)，`project.godot` 声明 features 为 `4.7`
- 无第三方依赖，只用引擎内置类型
- 不注册全局类名，各脚本按自己的 `preload` 路径引用；正常放一份 `nevext.gd` 即可（放多份也能各自引用，但属冗余）

## 目录结构

```
gdscript/
├── project.godot            # 工程配置（工程名 nevext-demo）
├── test.cmd                 # Windows 便捷入口：跑全部测试
├── demo.cmd                 # Windows 便捷入口：跑示例
├── icon.svg                 # 工程图标
├── nevext/
│   └── nevext.gd            # ★ 库本体，只需拷贝这个文件
├── docs/
│   └── test-plan.md         # GDScript 私有测试计划
├── demo/
│   └── demo01.gd            # 可运行示例（extends SceneTree）
└── test/
    ├── test_main.gd         # 测试入口（extends SceneTree）
    ├── test_framework.gd    # 零依赖断言框架
    ├── test_conformance.gd  # 跨实现通用语料（Conformance.A01 ~ H04）
    └── test_impl.gd         # GDScript 私有用例（Impl.*）
```

**运行注意事项：** `project.godot` 中**没有**设置 `run/main_scene`，
直接 `--path` 启动会报 `no main scene defined`，必须显式指定脚本：

```bash
# 跑全部测试；退出码 0 表示通过
godot --headless --path gdscript --script res://test/test_main.gd

# 只跑跨实现通用语料 / 某一组
... --script res://test/test_main.gd -- Conformance
... --script res://test/test_main.gd -- Conformance.C0

# 打印语料的实际结果，用于与 C++ 侧 diff
... --script res://test/test_main.gd -- --dump
```

`--` 之后才是脚本参数（`OS.get_cmdline_user_args()`），少了它 `--dump` 会被 Godot 当成引擎参数。
Windows 下也可直接双击 [`gdscript/test.cmd`](../../gdscript/test.cmd)。

## 快速上手

下面是完整可运行的示例：

```gdscript
extends SceneTree

const Nevext := preload("res://nevext/nevext.gd")

func _initialize() -> void:
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
	var res:Nevext.ParseResult = parser.parse_program()
	if res.err:
		print("==== Parser errors: ================================")
		for e in parser.errors:
			print(e)

	for i in range(len(res.val)):
		var s := res.val[i]
		print("statement ", i + 1, " at line ", s.line, ":")
		for st in s.strings:
			print('  ', st)
	quit(0)
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

> **GDScript 字面量会先转义一次**：源码写 `\"world\"`，交给 Nevext 时已是 `"world"`。
> 想测 Nevext 自己的转义就要再叠一层（`\\n` 才会变成交给 Nevext 的 `\n`）。

## API 参考

所有类型都是库脚本的内部类；`Nevext` 是你 `preload` 得到的脚本常量，通过 `Nevext.XXX` 访问。

版本常量 `Nevext.VERSION` 与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举

```gdscript
enum TokenType {
	Unexpected, EndOfFile, RawStr, QuotedStr, Sep,   # 与 C++ 的名称、顺序完全一致
}

class Result:
	var err:int = OK     # 0 = OK，1 = Err
	var val              # 携带的值
	const OK  = 0
	const Err = 1
```

> **要点：** 对外只有 `OK` / `Err` 两种结果，没有 C++ 内部那种
> `UnknownError` / `UnclosedStringError` 细分，无法从返回值区分错误类型；
> 要区分就得看 `Lexer.errors` / `Parser.errors`（见「错误处理」）。

### Token 与 Lexer

```gdscript
class Token:
	var type:TokenType
	var line:int
	var literal:String      # 未展开转义的原始内容（QuotedStr 会包含 \n 这类转义序列）

class Lexer:
	var escape_func:Callable   # 默认 nevext_escape；由 Parser 调用，只作用于 QuotedStr（可替换，见「转义函数」一节）
	var code:String
	var line:int = 1
	var errors:Array[String] = []

	func _init(code_:String="") -> void
	func reset(code_:String)
	func next_token() -> Token
```

一般不需要直接使用 `Lexer`，交给 `Parser` 驱动即可。

两个要点：

- `Lexer` 持有的是 `String`（值拷贝），**不存在** C++ 版本中「源字符串必须活得更久」的约束；
- **转义在语法层做**，不在词法层。`Token.literal` 是原始片段，展开由 `Parser` 通过
  `escape_func` 完成；因此 `RawStr` 与 `QuotedStr` 必须区分，这也是两者名字要和 C++ 一致的原因。

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
	func parse_program(pause_when_error:bool=true) -> ParseResult
```

**`parse_program` 参数**

| 参数 | 说明 |
| --- | --- |
| `pause_when_error` | `true`（默认）：遇到第一个错误立即停止解析；`false`：继续解析，尽量多收集错误 |

无论哪种模式，出错前已经解析出的语句都会保留在 `res.val` 中，且只要出过错
`res.err` 就是 `Err`（返回值与错误容器不会互相打脸）。

### 转义函数（可替换）

`Lexer.escape_func` 是 `Callable`，v0.4.0 起默认为 **Nevext 风格**的
`Nevext.nevext_escape`（原九项表 + `\uXXXX`，对 v0.3.0 及之前向后兼容；
见[语法概述](./overview.md#转义字符)）。三种内置函数：

```gdscript
var lexer := Nevext.Lexer.new(src)
lexer.escape_func = Nevext.null_escape     # 不转义，原样返回
lexer.escape_func = Nevext.nevext_escape    # 默认：九项 + \uXXXX
lexer.escape_func = Nevext.json_escape     # 严格 JSON 表（无 \' 与 \v，多 \/）
```

> v0.3.0 及之前默认的 `Nevext.defalt_escape`（拼写如此）已更名为
> `nevext_escape`（顺带新增 `\uXXXX` 支持），见[更新日志](../../CHANGELOG.md)。

实测：`null_escape` 下 `"a\nb"` 得到字面量 `a\nb`（保留反斜杠与 `n`）；
默认下 `"\u4e2d"` 得到 `中`、`'a\'b'` 得到 `a'b`，而 `\/` 与 `\q` 会报
`invalid escape character.`。

也可以传入自定义函数，签名需为 `func(s:String) -> Nevext.Result`：
成功时置 `res.val`，失败时置 `res.err = Nevext.Result.Err`。
转义失败时，Parser 会保留**已展开的前缀**并追加一条 `invalid escape character.` 错误，
这与 C++ 版 `Parser::escape` 的行为一致。

## 错误处理

### 错误容器有两个

与 C++ 版一致：`Lexer.errors`（词法层）与 `Parser.errors`（语法层）分开收集，
都形如 `'line N: <消息>'`。因此**只看 `Parser.errors` 会漏掉未闭合字符串这类词法错误**，
两边都要看。

| 消息 | 来源 | 行号含义 |
| --- | --- | --- |
| `line N: unclosed string literal.` | `Lexer.errors` | 字符串**扫描结束**所在行 |
| `line N: invalid escape character.` | `Parser.errors` | 该引号串**起始**所在行 |
| `line N: unexpected token.` | `Parser.errors` | 该 token **起始**所在行 |

措辞与 C++ 版完全相同，跨实现测试可以直接按消息归类（见 GDScript 侧 `kind_of`、
C++ 侧 `ErrorKind` 这两个映射函数）。

### 错误没有去重

两版一致：**都不做去重**。C++ 版的 `Parser::error()` 曾有行号去重（同一行只报第一条），
现已移除；GDScript 版从未有过这个逻辑。同一行制造两个错误（`pause_when_error = false`）时，
两条都会进 `errors`。

### 实测行为

| 输入 | `res.err` | 保留的语句 | 错误序列（行号, 种类） |
| --- | --- | --- | --- |
| `a; b; c`（末句无分号） | 0 | 3 条 | 无 |
| `;;;a;`（空语句） | 0 | 1 条 | 无（空语句被丢弃） |
| `a"b`（pause=true） | 1 | `[["a"]]` | `(1,UnclosedString) (1,UnexpectedToken)` |
| `a"b`（pause=false） | 1 | `[["a"]] [["b"]]` | 同上，后续内容仍被读出 |
| `"a\qb"; c`（pause=true） | 1 | `[["a"]]` | `(1,InvalidEscape)` |
| `a /* xx`（未闭合块注释） | 0 | `[["a"]]` | 无（语法规定：不是错误） |
| `a "b<换行>c`（跨行未闭合） | 1 | `[["a"]]` | `(2,UnclosedString) (1,UnexpectedToken)` |

完整清单见 [跨实现通用测试计划](./test-plan.md) 第 3 节；此处只摘取代表项。

### `errors` 不会自动清空

只有 `reset()` 会清空。实测：替换 `parser.lexer` 后不调用 `reset()` 再解析，
上一次的错误仍留在数组里。复用 `Parser` 前请调用 `parser.reset(new_lexer)`。

## 字符串编码

### Godot 的 String 无需转码

Godot 的 `String` 内部以 UTF-32 存储，可以直接处理中文与 emoji，
**不需要** C++ 版那样的 UTF-16 ↔ UTF-8 转换，解析结果可直接 `print`。

代价是：同一个非 BMP 字符（如 emoji）在 C++ 侧占 2 个 UTF-16 码元、在 GDScript 侧占 1 个码元，
**代码单元数不同**。跨实现测试因此只断言字符串内容相等，不断言长度，
见通用语料 H02。

### 控制台输出

Godot 的输出面板与 `print()` 均为 UTF-8，无需额外设置。
这与 C++ 版不同——C++ 在 Windows 控制台需要先调用 `SetConsoleOutputCP(CP_UTF8)`，否则中文乱码。

## 已修复的历史偏差

以下各项曾使 GDScript 版偏离 C++ 版，现已全部修复，每条都有回归用例：

| 曾有的偏差 | 现状 |
| --- | --- |
| `TokenType` 只有 4 项（用 `EOF` / `Str`），且不区分裸串与引号串 | 与 C++ 逐项一致：`Unexpected, EndOfFile, RawStr, QuotedStr, Sep` |
| 转义在词法层做，`Token.literal` 是已展开的内容 | 改为在 `Parser` 层展开，`Token.literal` 保留原文 |
| 转义后截串用了错的下标，`"a\nb"` 静默丢掉 `b` 且不报错 | 修为 `s.substr(2)`；现在是 `a` + 换行 + `b` |
| 裸串遇 `"` `'` `//` `/*` 不中断 | 补 `raw_str_end_chars` 与注释提前终止，与 C++ 的 `rawStrEndChars` 逐项一致 |
| 未闭合字符串吞掉剩余输入且不报错 | 报错、保留语句，剩余内容继续被读出（不再吞掉） |
| 引号串内的换行不推进行号 | 每个物理换行都计数，含被反斜杠转义的那个换行 |
| 非法转义时把已展开的内容丢成空串 | 保留失败点之前的前缀，与 C++ 一致 |
| `Lexer` 没有错误容器 | 新增 `Lexer.errors`，与 `Parser.errors` 分工同 C++ |
| 单行注释靠 `substr(-1)` 的隐式语义跳过尾部 | 改为显式 `find()` 判断 |

## 已知缺口

都不影响与 C++ 的行为一致性，属于「接口之外的差异」：

| 编号 | 现象 | 处置 |
| --- | --- | --- |
| **X1** | 转义表里没有「`\` + 换行」，引号串内写续行最终报 `InvalidEscape` | 两侧行为一致，由语料 C08 / F04 固化，详见通用测试计划 |
| **X2** | 非 BMP 字符的内部码元数不同（UTF-32 vs UTF-16） | 只比较字符串内容，不比较长度 |
| **X3** | 性能随规模急剧劣化：`Lexer` 每推进一个字符做一次 `code.substr()`，是超线性量级 | 属实现固有代价，长输入用例只在 GDScript 私有套件里做小规模版本 |

### 性能参考（headless 实测）

| 输入字符数 | 语句数 | 解析耗时 |
| --- | --- | --- |
| 1.7 万 | 850 | 24 ms |
| 7.3 万 | 3,550 | 204 ms |
| 14.6 万 | 7,000 | 1,140 ms |

字符数翻倍时耗时增长约 5 倍，明显超线性。KB 级以上的输入请务必评估，或改用 C++ 实现。

## 生命周期与常见陷阱

| 陷阱 | 说明 |
| --- | --- |
| **集成方式** | 不注册全局类名；每个使用它的脚本顶部都要 `const Nevext := preload(...)` |
| **工程无主场景** | 未设 `run/main_scene`，运行时必须显式传脚本路径 |
| **不依赖 `.godot/` 缓存** | v0.3.0 及之前靠 `global_script_class_cache.cfg` 注册全局类，删了 `.godot/` 就报 "Nevext 未声明"；v0.4.0 起改为 `preload`，无缓存副本实测测试与 demo 照常运行 |
| **`errors` 需手动清** | 只有 `reset()` 会清空，见「错误处理」一节 |
| **只见 `Parser.errors` 会漏错** | 未闭合字符串在 `Lexer.errors` 里 |
| **GDScript 字面量会先转义一次** | 源码写 `\"world\"`，交给 Nevext 时已是 `"world"` |
| **大文件性能** | 见上一节，14.6 万字符约 1.1 秒 |
| **逗号等同空格** | `a, b` 与 `a b` 解析结果相同，这是语法规定而非 bug |
| **脚本参数要加 `--`** | `--script x.gd -- --dump`；否则引擎自己吃掉参数 |

## 运行示例

[`gdscript/demo/demo01.gd`](../../gdscript/demo/demo01.gd) 是可直接运行的完整示例，
与 C++ / GDScript / JavaScript / C# / Python 五份 demo 使用同一段输入与同一输出格式：

```bash
# 内置示例
godot --headless --path gdscript --script res://demo/demo01.gd

# 解析指定文件（UTF-8）；注意 -- 之后才是脚本参数
godot --headless --path gdscript --script res://demo/demo01.gd -- 某个文件.txt

# Windows 便捷入口
gdscript\demo.cmd [文件]
```

> **相对路径的基准是 Godot 工程目录**：带 `--path gdscript` 时，`FileAccess.open()` 的相对路径
> 解析到 `gdscript/`，而不是调用者的当前目录。传相对路径找不到文件时请改用绝对路径
> （`gdscript\demo.cmd` 已先 `cd` 到工程目录，同样遵守这一规则）。

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.errors` 与 `parser.errors`
两个容器）；`2` 文件打不开。五份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 测试

- 跨实现通用语料：`gdscript/test/test_conformance.gd`，60 条，定义见
  [docs/zh-cn/test-plan.md](./test-plan.md)
- GDScript 私有用例：`gdscript/test/test_impl.gd`，覆盖可替换的 `escape_func`、实例复用、
  终止性（病态串的全部前缀）、长裸串、大量语句
- 计划与基线：[gdscript/docs/test-plan.md](../../gdscript/docs/test-plan.md)
- 与 C++ 侧对照：见通用测试计划第 5 节的 `--dump` + `diff --strip-trailing-cr`，
  以及第 5.1 节的 demo 门禁

## 许可

本项目使用 MIT 协议，详见仓库根目录的 [LICENSE](../../LICENSE)。

> AI生成
