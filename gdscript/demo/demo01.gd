extends SceneTree
# ---------------------------------------------------------------------------
# demo01.gd —— Nevext GDScript 实现的示例程序。
#
# 运行（工程根目录为 gdscript/）：
#   godot --headless --path gdscript --script res://demo/demo01.gd
#   godot --headless --path gdscript --script res://demo/demo01.gd -- 某个文件.txt
# 或直接双击 gdscript/demo.cmd（传入文件路径：gdscript\demo.cmd 某个文件.txt）。
#
# 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此五个实现
# （C++ / GDScript / JavaScript / C# / Python）的 demo 输出可以互相 diff，
# 见 docs/zh-cn/test-plan.md 的「demo 门禁」。
#
# 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
#
# 注意：工程未设 run/main_scene，只能以 --script 方式启动，故必须
# extends SceneTree 并实现 _initialize()。
# ---------------------------------------------------------------------------

# 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
const BUILTIN := '  // 单行注释\n  hello "world";\n  /*            \n  多行注释      \n  */            \n  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾'


# 无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 null。
func _read_input():
	# --script 模式下，`--` 之后的参数才是脚本参数
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		return BUILTIN
	var path: String = args[0]
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		# 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
		printerr('cannot open: ' + path)
		return null
	var text := file.get_as_text()
	file.close()
	# 去掉可能存在的 UTF-8 BOM，避免它被当成输入的第一个字符
	if text.begins_with('\uFEFF'):
		text = text.substr(1)
	return text


func _initialize() -> void:
	var input = _read_input()
	if input == null:
		quit(2)
		return

	var lexer := Nevext.Lexer.new(input)
	var parser := Nevext.Parser.new(lexer)
	var res: Nevext.ParseResult = parser.parse_program()

	if res.err:
		# 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
		print('==== Lexer errors =============================')
		for e in lexer.errors:
			print(e)
		print('==== Parser errors =============================')
		for e in parser.errors:
			print(e)
		quit(1)
		return

	print('statements num: ' + str(res.val.size()))
	for i in range(res.val.size()):
		var stmt: Nevext.Statement = res.val[i]
		print('statement ' + str(i + 1) + ' at line ' + str(stmt.line) + ':')
		# 与 C++ 一致：每个串后跟一个空格
		var line := '  '
		for s in stmt.strings:
			line += s + ' '
		print(line)
	quit(0)
