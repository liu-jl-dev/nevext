extends RefCounted
# 跨实现通用语料测试
#
# 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
# cpp/test/test_conformance.cpp。两边必须完全一致：ID、输入、pause 开关、
# 期望的返回值/产物/错误序列。
#
# 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。
#
# 转义记法：下表一律用 GDScript 转义书写 \n \r \t \v \b \f \\ \" \'，
# 这样本文件保持纯 ASCII 语义，不会因为 .gd 的编码解释不同而漂移。
# 非 ASCII 只在 H04 一处用了字面量（Godot 恒定按 UTF-8 读 .gd）。

const Framework := preload("res://test/test_framework.gd")

# 每条语料：id / src / pause / ok(是否期望成功) / prog / errs
#   prog 为 [[行号, [字符串...]], ...]
#   errs 为 [[行号, 语义种类], ...]，顺序敏感
# 写成函数而不是常量：表里用到了 char(...) 构造不可见字符，而 GDScript 的 const
# 不允许在常量表达式里调用内置函数。每次重新构建这张表的开销可以忽略。
static func cases() -> Array:
	return [
	# ---- A 组 程序骨架与语句切分
	{"id": "A01", "src": "",                          "pause": true,  "ok": true,  "prog": [], "errs": []},
	{"id": "A02", "src": "abc",                       "pause": true,  "ok": true,  "prog": [[1, ["abc"]]], "errs": []},
	{"id": "A03", "src": "a, b\tc",                   "pause": true,  "ok": true,  "prog": [[1, ["a", "b", "c"]]], "errs": []},
	{"id": "A04", "src": "a b; c d; e",               "pause": true,  "ok": true,  "prog": [[1, ["a", "b"]], [1, ["c", "d"]], [1, ["e"]]], "errs": []},
	{"id": "A05", "src": "a; b",                      "pause": true,  "ok": true,  "prog": [[1, ["a"]], [1, ["b"]]], "errs": []},
	{"id": "A06", "src": ";;;a;",                     "pause": true,  "ok": true,  "prog": [[1, ["a"]]], "errs": []},
	{"id": "A07", "src": "a,,b",                      "pause": true,  "ok": true,  "prog": [[1, ["a", "b"]]], "errs": []},
	{"id": "A08", "src": "  \t\n ",                   "pause": true,  "ok": true,  "prog": [], "errs": []},
	{"id": "A09", "src": "a b\nc d;",                 "pause": true,  "ok": true,  "prog": [[1, ["a", "b", "c", "d"]]], "errs": []},
	{"id": "A10", "src": "windowSize 1280, 720;",     "pause": true,  "ok": true,  "prog": [[1, ["windowSize", "1280", "720"]]], "errs": []},

	# ---- B 组 引号串
	{"id": "B01", "src": "\"hello world\" x",         "pause": true,  "ok": true,  "prog": [[1, ["hello world", "x"]]], "errs": []},
	{"id": "B02", "src": "'abc'",                     "pause": true,  "ok": true,  "prog": [[1, ["abc"]]], "errs": []},
	{"id": "B03", "src": "\"\" z",                    "pause": true,  "ok": true,  "prog": [[1, ["", "z"]]], "errs": []},
	{"id": "B04", "src": "'a' \"b\" c",               "pause": true,  "ok": true,  "prog": [[1, ["a", "b", "c"]]], "errs": []},
	{"id": "B05", "src": "'\"' \"'\"",                "pause": true,  "ok": true,  "prog": [[1, ["\"", "'"]]], "errs": []},
	{"id": "B06", "src": "\"a\"b",                    "pause": true,  "ok": true,  "prog": [[1, ["a", "b"]]], "errs": []},

	# ---- C 组 转义（只在语法层对 QuotedStr 展开）
	{"id": "C01", "src": "\"a\\nb\"",                 "pause": true,  "ok": true,  "prog": [[1, ["a\nb"]]], "errs": []},
	{"id": "C02", "src": "\"\\n\\r\\t\\v\\b\\f\"",    "pause": true,  "ok": true,  "prog": [[1, ["\n\r\t\v\b\f"]]], "errs": []},
	{"id": "C03", "src": "\"a\\\\\"",                 "pause": true,  "ok": true,  "prog": [[1, ["a\\"]]], "errs": []},
	{"id": "C04", "src": "\"ab\\\"cd\"",              "pause": true,  "ok": true,  "prog": [[1, ["ab\"cd"]]], "errs": []},
	{"id": "C05", "src": "'a\\qb'",                   "pause": true,  "ok": false, "prog": [[1, ["a"]]], "errs": [[1, "InvalidEscape"]]},
	{"id": "C06", "src": "\"a\\qb\"; c",              "pause": true,  "ok": false, "prog": [[1, ["a"]]], "errs": [[1, "InvalidEscape"]]},
	{"id": "C07", "src": "\"a\\qb\"; c",              "pause": false, "ok": false, "prog": [[1, ["a"]], [1, ["c"]]], "errs": [[1, "InvalidEscape"]]},
	# X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
	{"id": "C08", "src": "\"a\\\nb\"",                "pause": true,  "ok": false, "prog": [[1, ["a"]]], "errs": [[1, "InvalidEscape"]]},
	{"id": "C09", "src": "\"x\\ny\\tz\"",             "pause": true,  "ok": true,  "prog": [[1, ["x\ny\tz"]]], "errs": []},

	# ---- D 组 裸串边界（裸串不展开转义）
	{"id": "D01", "src": "a\\b",                      "pause": true,  "ok": true,  "prog": [[1, ["a\\b"]]], "errs": []},
	{"id": "D02", "src": "a\\nb",                     "pause": true,  "ok": true,  "prog": [[1, ["a\\nb"]]], "errs": []},
	{"id": "D03", "src": "a\\qb",                     "pause": true,  "ok": true,  "prog": [[1, ["a\\qb"]]], "errs": []},
	{"id": "D04", "src": "a\"b",                      "pause": true,  "ok": false, "prog": [[1, ["a"]]],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "D05", "src": "a\"b",                      "pause": false, "ok": false, "prog": [[1, ["a"]], [1, ["b"]]],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "D06", "src": "a'b",                       "pause": false, "ok": false, "prog": [[1, ["a"]], [1, ["b"]]],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "D07", "src": "a//comment",                "pause": true,  "ok": true,  "prog": [[1, ["a"]]], "errs": []},
	{"id": "D08", "src": "a/**/ b",                   "pause": true,  "ok": true,  "prog": [[1, ["a", "b"]]], "errs": []},
	{"id": "D09", "src": "\"a//b\"",                  "pause": true,  "ok": true,  "prog": [[1, ["a//b"]]], "errs": []},
	# 退格不是裸串终止字符：整串是一个 RawStr
	{"id": "D10", "src": "a" + char(0x08) + "b",      "pause": false, "ok": true,  "prog": [[1, ["a\bb"]]], "errs": []},

	# ---- E 组 注释（未闭合的多行注释不是错误）
	{"id": "E01", "src": "// only comment",           "pause": true,  "ok": true,  "prog": [], "errs": []},
	{"id": "E02", "src": "/* c1\nc2 */ d",            "pause": true,  "ok": true,  "prog": [[2, ["d"]]], "errs": []},
	{"id": "E03", "src": "a /* xx",                   "pause": true,  "ok": true,  "prog": [[1, ["a"]]], "errs": []},
	{"id": "E04", "src": "a /* x\nb; c;",             "pause": true,  "ok": true,  "prog": [[1, ["a"]]], "errs": []},
	{"id": "E05", "src": "/**/a",                     "pause": true,  "ok": true,  "prog": [[1, ["a"]]], "errs": []},
	{"id": "E06", "src": "a //c1\n/* c2 */ b",        "pause": true,  "ok": true,  "prog": [[1, ["a", "b"]]], "errs": []},

	# ---- F 组 行号
	{"id": "F01", "src": "a;\nb;\nc;",                "pause": true,  "ok": true,  "prog": [[1, ["a"]], [2, ["b"]], [3, ["c"]]], "errs": []},
	{"id": "F02", "src": "a;\r\nb;",                  "pause": true,  "ok": true,  "prog": [[1, ["a"]], [2, ["b"]]], "errs": []},
	{"id": "F03", "src": "\"a\nb\"; c;",              "pause": true,  "ok": true,  "prog": [[1, ["a\nb"]], [2, ["c"]]], "errs": []},
	{"id": "F04", "src": "\"a\\\nb\"; c;",            "pause": true,  "ok": false, "prog": [[1, ["a"]]], "errs": [[1, "InvalidEscape"]]},
	# 词法错误报在串结束行，语法错误报在串起始行
	{"id": "F05", "src": "a \"b\nc",                  "pause": true,  "ok": false, "prog": [[1, ["a"]]],
		"errs": [[2, "UnclosedString"], [1, "UnexpectedToken"]]},

	# ---- G 组 错误与错误恢复
	{"id": "G01", "src": "a \"bc",                    "pause": true,  "ok": false, "prog": [[1, ["a"]]],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "G02", "src": "a \"bc",                    "pause": false, "ok": false, "prog": [[1, ["a"]], [1, ["bc"]]],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	# 同一行的多处错误都要记录
	{"id": "G03", "src": "\"a\\qb\" \"c\\qd\"; e",    "pause": false, "ok": false, "prog": [[1, ["a", "c"]], [1, ["e"]]],
		"errs": [[1, "InvalidEscape"], [1, "InvalidEscape"]]},
	{"id": "G04", "src": "\"abc",                     "pause": true,  "ok": false, "prog": [],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "G05", "src": "\"a\nb",                    "pause": true,  "ok": false, "prog": [],
		"errs": [[2, "UnclosedString"], [1, "UnexpectedToken"]]},

	# ---- H 组 编码与其它
	{"id": "H01", "src": "a;\"b\";c",                 "pause": true,  "ok": true,  "prog": [[1, ["a"]], [1, ["b"]], [1, ["c"]]], "errs": []},
	# 非 BMP 字符：GDScript 内部为 UTF-32 单码元，C++ 为 UTF-16 代理对，内容须相等
	{"id": "H02", "src": "\"" + char(0x1F600) + "\" x", "pause": true,  "ok": true,
		"prog": [[1, [char(0x1F600), "x"]]], "errs": []},
	{"id": "H03", "src": "\"",                        "pause": true,  "ok": false, "prog": [],
		"errs": [[1, "UnclosedString"], [1, "UnexpectedToken"]]},
	{"id": "H04", "src": "海上生明月 天涯共此时", "pause": true, "ok": true,
		"prog": [[1, ["海上生明月", "天涯共此时"]]], "errs": []},
	]


# ------------------------------------------------------------------ 注册
# 套件实例的持有者，防止被引用计数释放，见 register() 里的说明
static var _suite: RefCounted = null


static func register() -> void:
	# 套件实例必须挂在静态变量上：只放局部变量的话 register() 返回后 RefCounted 引用归零被释放，
	# 绑出来的 Callable 就变成空对象调用了。
	if _suite == null:
		# load() 返回无静态类型的 Variant，必须显式标注，否则 `var x := load(...)` 会因类型
		# 无法推断而整个文件解析失败（Godot 的报错信息只在 check-only 下才看得见）
		var self_script: Script = load("res://test/test_conformance.gd")
		_suite = self_script.new()
	for c in cases():
		Framework.add_case("Conformance." + str(c["id"]), _suite.check_case.bind(c))


# ------------------------------------------------------------ 运行单条语料
func check_case(ctx: Dictionary, c: Dictionary) -> void:
	var src: String = c["src"]
	var lexer := Nevext.Lexer.new(src)
	var parser := Nevext.Parser.new(lexer)
	var res = parser.parse_prog(bool(c["pause"]))

	# 1) 返回值
	Framework.check_eq(ctx, "Success" if bool(c["ok"]) else "Error",
		"Success" if res.err == 0 else "Error", "返回值", "实际返回值")

	# 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
	var prog: Array = res.val
	var want_prog: Array = c["prog"]
	Framework.check_eq(ctx, want_prog.size(), prog.size(), "语句数", "实际语句数")
	var n := mini(prog.size(), want_prog.size())
	for i in n:
		var stmt = prog[i]
		Framework.check_eq(ctx, want_prog[i][0], stmt.line, "语句[%d]行号" % i, "实际行号")
		var want_strs: Array = want_prog[i][1]
		Framework.check_eq(ctx, want_strs.size(), stmt.strings.size(),
			"语句[%d]字符串数" % i, "实际字符串数")
		var m := mini(stmt.strings.size(), want_strs.size())
		for j in m:
			Framework.check_eq(ctx, want_strs[j], stmt.strings[j],
				"语句[%d]第[%d]个字符串" % [i, j], "实际值")

	# 3) 错误：按 (行号,种类) 序列比较，顺序敏感
	var got := sem_errors(lexer, parser)
	var want_errs: Array = c["errs"]
	Framework.check_eq(ctx, want_errs.size(), got.size(), "错误数", "实际错误数")
	var k := mini(got.size(), want_errs.size())
	for i in k:
		Framework.check_eq(ctx, "%d:%s" % [want_errs[i][0], want_errs[i][1]], got[i],
			"第[%d]条错误" % i, "实际错误")


# -------------------------------------------------------------------- dump
# 按 docs/zh-cn/test-plan.md 第 5 节的行格式打印全部语料的实际结果，
# 与 cpp 侧的 `nevext_test --dump` 逐行对照。
static func dump() -> void:
	for c in cases():
		var lexer := Nevext.Lexer.new(c["src"] as String)
		var parser := Nevext.Parser.new(lexer)
		var res = parser.parse_prog(bool(c["pause"]))

		var prog_parts: Array[String] = []
		for stmt in res.val:
			var parts: Array[String] = []
			for s in stmt.strings:
				parts.append("\"" + dump_quote(Framework.escape_visible(s)) + "\"")
			prog_parts.append("%d:[%s]" % [stmt.line, ",".join(parts)])

		var got := sem_errors(lexer, parser)
		print("%s|%s|%s|%s|pause=%d" % [
			c["id"],
			"OK" if res.err == 0 else "Err",
			"|".join(prog_parts),
			",".join(got),
			1 if bool(c["pause"]) else 0,
		])


# ------------------------------------------------------------------ 辅助
# 错误消息 -> 语义种类。改消息措辞不需要改用例。
static func kind_of(msg: String) -> String:
	if msg.contains("unclosed string literal"): return "UnclosedString"
	if msg.contains("invalid escape character"): return "InvalidEscape"
	if msg.contains("unexpected token"): return "UnexpectedToken"
	return "RAW:" + msg


static func line_of(msg: String) -> int:
	var p := msg.find("line ")
	if p < 0: return -1
	var digits := ""
	var rest := msg.substr(p + 5)
	for ch in rest:
		var s := ch as String
		if s.is_valid_int(): digits += s
		else: break
	return digits.to_int()


# 词法错误在前、语法错误在后，与 C++ 侧 SemErrors() 的顺序一致
static func sem_errors(lexer, parser) -> Array[String]:
	var list: Array[String] = []
	for e in lexer.errors:
		list.append("%d:%s" % [line_of(e), kind_of(e)])
	for e in parser.errors:
		list.append("%d:%s" % [line_of(e), kind_of(e)])
	return list


# dump 用：把串内的引号转义掉，保证每行都能被机械解析。
# 必须在 escape_visible 之后调用（此时反斜杠已翻倍），否则会把已有的反斜杠吞掉。
static func dump_quote(s: String) -> String:
	var out := ""
	for i in s.length():
		var ch: String = s[i]
		if ch == "\"": out += "\\\""
		else: out += ch
	return out
