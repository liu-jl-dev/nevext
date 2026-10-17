extends SceneTree
# nevext GDScript 单元测试入口
#
# 用法（工程根目录为 gdscript/）：
#   <Godot> --headless --path . --script res://test/test_main.gd
#   ... --script res://test/test_main.gd -- Conformance      # 只跑通用语料
#   ... --script res://test/test_main.gd -- Conformance.C0   # 按 ID 前缀筛选
#   ... --script res://test/test_main.gd -- --dump           # 打印语料实际结果
#
# `--` 之后的参数才是脚本参数（OS.get_cmdline_user_args），否则会被 Godot 自己吃掉。
# 退出码：0 全部通过，1 有失败 —— 可直接接 CI。

const Framework := preload("res://test/test_framework.gd")
const Conformance := preload("res://test/test_conformance.gd")
const Impl := preload("res://test/test_impl.gd")


func _initialize() -> void:
	var argv := OS.get_cmdline_user_args()
	var filter := ""
	var verbose := false
	var dump := false
	for raw in argv:
		var arg := raw as String
		if arg == "-v" or arg == "--verbose":
			verbose = true
		elif arg == "--dump":
			dump = true
		elif arg != "":
			filter = arg

	if dump:
		Conformance.dump()
		quit(0)
		return

	Framework.reset()
	Conformance.register()  # 跨实现通用语料（docs/zh-cn/test-plan.md）
	Impl.register()         # GDScript 私有用例（gdscript/docs/test-plan.md）
	var code := Framework.run_all(filter, verbose)
	quit(code)
