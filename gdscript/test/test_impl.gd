extends RefCounted
# GDScript 实现私有测试
#
# 这里放「只有 GDScript 实现才有 / 通用语料不好表达」的用例：可替换的 escape_func、
# 实例复用、终止性、长输入。跨实现的那部分语料在 test_conformance.gd。

const Framework := preload("res://test/test_framework.gd")


# ------------------------------------------------------------------ 注册
static var _suite: RefCounted = null


static func register() -> void:
	if _suite == null:
		var self_script: Script = load("res://test/test_impl.gd")
		_suite = self_script.new()
	for name in ["EscapeFuncCanBeReplaced", "LexerAndParserResetAreReusable",
			"PathologicalInputsTerminate", "LongRawTokenIsComplete",
			"ManyStatementsAreAllParsed"]:
		Framework.add_case("Impl." + name, Callable(_suite, name))


# IMP-01 escape_func 是可替换的扩展点（C++ 实现没有这一层）
func EscapeFuncCanBeReplaced(ctx: Dictionary) -> void:
	var lexer := Nevext.Lexer.new("\"a\\nb\" x")
	lexer.escape_func = Nevext.null_escape
	var parser := Nevext.Parser.new(lexer)
	var res = parser.parse_prog(true)
	Framework.check_eq(ctx, "Success", "Success" if res.err == 0 else "Error", "返回值", "实际返回值")
	var prog: Array = res.val
	Framework.check_eq(ctx, 1, prog.size(), "语句数", "实际语句数")
	if prog.size() >= 1:
		Framework.check_eq(ctx, "a\\nb", prog[0].strings[0], "未经转义展开的字符串", "实际值")
	Framework.check_eq(ctx, 0, parser.errors.size(), "错误数", "实际错误数")


# IMP-02 Lexer.reset / Parser.reset 能让实例重用到另一份源码
func LexerAndParserResetAreReusable(ctx: Dictionary) -> void:
	var lexer := Nevext.Lexer.new("a b")
	var parser := Nevext.Parser.new(lexer)
	var first = parser.parse_prog(true)
	Framework.check_eq(ctx, 1, (first.val as Array).size(), "第一次语句数", "实际语句数")

	lexer.reset("c; d")
	parser.reset(lexer)
	var second = parser.parse_prog(true)
	var prog: Array = second.val
	Framework.check_eq(ctx, 2, prog.size(), "第二次语句数", "实际语句数")
	if prog.size() >= 2:
		Framework.check_eq(ctx, "c", prog[0].strings[0], "第 1 条语句", "实际值")
		Framework.check_eq(ctx, "d", prog[1].strings[0], "第 2 条语句", "实际值")
	Framework.check_eq(ctx, 1, lexer.line, "词法器起始行号", "实际行号")


# IMP-03 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
func PathologicalInputsTerminate(ctx: Dictionary) -> void:
	var src := "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;"
	for i in src.length() + 1:
		var lexer := Nevext.Lexer.new(src.substr(0, i))
		var reached_eof := false
		for step in 200:
			if lexer.next_token().type == Nevext.TokenType.EndOfFile:
				reached_eof = true
				break
		Framework.check_true(ctx, reached_eof, "前缀[%d]能在阈值内扫到 EOF" % i)


# IMP-04 长裸串不被截断
func LongRawTokenIsComplete(ctx: Dictionary) -> void:
	var src := "x".repeat(1000) + ";"
	var lexer := Nevext.Lexer.new(src)
	var parser := Nevext.Parser.new(lexer)
	var res = parser.parse_prog(true)
	Framework.check_eq(ctx, "Success", "Success" if res.err == 0 else "Error", "返回值", "实际返回值")
	var prog: Array = res.val
	if prog.size() >= 1:
		Framework.check_eq(ctx, 1000, (prog[0].strings[0] as String).length(),
			"裸串长度", "实际长度")


# IMP-05 大量语句：数量、内容、行号都要正确
func ManyStatementsAreAllParsed(ctx: Dictionary) -> void:
	var lines: Array[String] = []
	for i in 500:
		lines.append("s%d a b;" % i)
	var src := "\n".join(lines)
	var lexer := Nevext.Lexer.new(src)
	var parser := Nevext.Parser.new(lexer)
	var res = parser.parse_prog(true)
	Framework.check_eq(ctx, "Success", "Success" if res.err == 0 else "Error", "返回值", "实际返回值")
	var prog: Array = res.val
	Framework.check_eq(ctx, 500, prog.size(), "语句数", "实际语句数")
	if prog.size() >= 500:
		Framework.check_eq(ctx, "s0", prog[0].strings[0], "首条语句内容", "实际值")
		Framework.check_eq(ctx, 1, prog[0].line, "首条语句行号", "实际行号")
		Framework.check_eq(ctx, "s499", prog[499].strings[0], "末条语句内容", "实际值")
		Framework.check_eq(ctx, 500, prog[499].line, "末条语句行号", "实际行号")
