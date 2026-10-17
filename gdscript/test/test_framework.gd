extends RefCounted
# 极简单元测试框架（零第三方依赖）
#
# 设计与 cpp/test/test_framework.hpp 对齐：按名字筛选、失败不中断、输出与汇总口径一致，
# 方便两侧逐条对照。两处差异：
#   · GDScript 拿不到调用点的文件名与行号，失败信息里没有 file:line，所以断言一律带上
#     「期望是什么」的中文说明来自我定位；
#   · 上下文用 Dictionary 而不是类实例：GDScript 的内部类访问外层脚本成员不可靠，
#     改成全静态函数 + 显式传 ctx 就没有作用域歧义。
#
# 用法：
#   const Framework := preload("res://test/test_framework.gd")
#   Framework.add_case("Suite.Case", callable.bind(...))
#   var code := Framework.run_all(filter, verbose)

# --------------------------------------------------------------- 值的文本化
# 控制字符转成可见转义，避免诊断信息把终端刷乱。必须先转义反斜杠本身。
static func escape_visible(s: String) -> String:
	var out := ""
	for i in s.length():
		var c: String = s[i]
		var code: int = s.unicode_at(i)
		if c == "\\": out += "\\\\"
		elif c == "\n": out += "\\n"
		elif c == "\r": out += "\\r"
		elif c == "\t": out += "\\t"
		elif c == "\v": out += "\\v"
		elif c == "\b": out += "\\b"
		elif c == "\f": out += "\\f"
		elif code < 32 or code == 127: out += "\\x%02X" % code
		else: out += c
	return out


static func to_str(v) -> String:
	if v is String:
		return "\"" + escape_visible(v) + "\""
	return str(v)


# ------------------------------------------------------------------ 断言
static func new_context(name: String) -> Dictionary:
	return {"name": name, "checks": 0, "failures": 0}


static func check_true(ctx: Dictionary, ok: bool, expr: String) -> void:
	ctx.checks += 1
	if not ok:
		_fail(ctx, "期望为真: " + expr)


static func check_eq(ctx: Dictionary, expected, actual, expr_expected: String,
		expr_actual: String) -> void:
	ctx.checks += 1
	if expected != actual:
		_fail(ctx, "期望 %s == %s\n           期望值: %s\n           实际值: %s" % [
			expr_expected, expr_actual, to_str(expected), to_str(actual),
		])


static func _fail(ctx: Dictionary, detail: String) -> void:
	ctx.failures += 1
	print("    [FAIL] " + detail)


# ------------------------------------------------------------------ 用例注册表
static var _cases: Array = []


static func reset() -> void:
	_cases.clear()


static func add_case(name: String, fn: Callable) -> void:
	_cases.append({"name": name, "fn": fn})


static func case_count() -> int:
	return _cases.size()


# -------------------------------------------------------------------- 执行入口
# filter：只运行名字包含该子串的用例；verbose：预留，输出口径与 C++ 侧一致
# 返回值：0 全部通过，1 有失败
static func run_all(filter: String = "", verbose: bool = false) -> int:
	print("==== nevext GDScript 单元测试 ====")
	var case_passed := 0
	var case_failed := 0
	var total_checks := 0
	var total_failures := 0
	for c in _cases:
		var name: String = c["name"]
		if filter != "" and not name.contains(filter):
			continue
		var ctx := new_context(name)
		print("[ RUN  ] " + name)
		(c["fn"] as Callable).call(ctx)
		total_checks += ctx.checks
		total_failures += ctx.failures
		if ctx.failures != 0:
			case_failed += 1
			print("[ FAIL ] %s   失败断言 %d/%d" % [name, ctx.failures, ctx.checks])
		else:
			case_passed += 1
			print("[  OK  ] %s   %d 个断言通过" % [name, ctx.checks])
	print("--------------------------------------------------")
	print("用例: %d，通过: %d，失败: %d" % [case_passed + case_failed, case_passed, case_failed])
	print("断言: %d，失败: %d" % [total_checks, total_failures])
	if case_failed != 0:
		print("结果: 失败")
		return 1
	print("结果: 全部通过")
	return 0
