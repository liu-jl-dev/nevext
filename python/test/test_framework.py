# ---------------------------------------------------------------------------
# test_framework.py —— 极简单元测试框架（零第三方依赖）
#
# 设计取舍与 cpp/test/test_framework.hpp 一致：
#   · 不引入任何测试框架（不用 unittest / pytest）：nevext 保持零依赖，
#     python 一条命令就能跑；
#   · 失败只累加计数并打印，单个断言失败不影响后续用例；
#   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
# ---------------------------------------------------------------------------

import sys


class Context:
    """单个用例的运行上下文：累加断言数与失败数。"""

    __slots__ = ("name", "checks", "failures")

    def __init__(self, name):
        self.name = name
        self.checks = 0
        self.failures = 0


_REGISTRY = []


def register(name, fn):
    _REGISTRY.append((name, fn))


def show(s):
    """把控制字符转成可见转义，避免诊断信息把终端刷乱。
    与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：
    六个具名控制字符用 \\n \\r \\t \\v \\b \\f，反斜杠翻倍，其余控制字符 \\xNN。"""
    out = []
    for ch in s:
        code = ord(ch)
        if ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\v":
            out.append("\\v")
        elif ch == "\b":
            out.append("\\b")
        elif ch == "\f":
            out.append("\\f")
        elif ch == "\\":
            out.append("\\\\")
        elif code < 0x20 or code == 0x7F:
            out.append("\\x" + format(code, "02X"))
        else:
            out.append(ch)
    return "".join(out)


def quote(v):
    return '"' + show(str(v)) + '"'


def _report_failure(ctx, label, detail):
    ctx.failures += 1
    print("    [FAIL] " + label + "\n           " + detail)


def check_true(ctx, cond, label):
    ctx.checks += 1
    if not cond:
        _report_failure(ctx, label, "期望为真")


def check_false(ctx, cond, label):
    ctx.checks += 1
    if cond:
        _report_failure(ctx, label, "期望为假")


def check_eq(ctx, expected, actual, label):
    ctx.checks += 1
    if expected != actual:
        exp_text = quote(expected) if isinstance(expected, str) else str(expected)
        act_text = quote(actual) if isinstance(actual, str) else str(actual)
        _report_failure(ctx, label, "期望 " + exp_text + "，实际 " + act_text)


def run_all(argv):
    """全部用例跑一遍；argv 支持 <名字子串> 与 -v。返回退出码。"""
    filter_text = ""
    for arg in argv:
        if arg in ("-v", "--verbose"):
            pass  # 详细模式占位：本框架已逐用例打印，保留参数只为兼容
        elif arg != "":
            filter_text = arg

    case_failed = 0
    case_passed = 0
    total_checks = 0
    total_failures = 0

    print("==== nevext Python 单元测试 ====")
    for name, fn in _REGISTRY:
        if filter_text != "" and filter_text not in name:
            continue
        ctx = Context(name)
        print("[ RUN  ] " + name)
        try:
            fn(ctx)
        except Exception as exc:  # noqa: BLE001 —— 用例异常要计入失败而不是中断整轮
            ctx.failures += 1
            print("    [FAIL] 用例抛出异常: " + repr(exc))
        total_checks += ctx.checks
        total_failures += ctx.failures
        if ctx.failures != 0:
            case_failed += 1
            print("[ FAIL ] " + name + "   失败断言 " + str(ctx.failures) + "/" + str(ctx.checks))
        else:
            case_passed += 1
            print("[  OK  ] " + name + "   " + str(ctx.checks) + " 个断言通过")

    print(
        "--------------------------------------------------\n"
        + "用例: "
        + str(case_passed + case_failed)
        + "，通过: "
        + str(case_passed)
        + "，失败: "
        + str(case_failed)
        + "\n"
        + "断言: "
        + str(total_checks)
        + "，失败: "
        + str(total_failures)
    )
    if case_failed != 0:
        print("结果: 失败")
        return 1
    print("结果: 全部通过")
    return 0


def reset():
    """清空注册表（只给需要重复注册的极端场景用，正常流程不需要）。"""
    del _REGISTRY[:]


def _self_check():
    """框架自检：确保 show 的转义与 C++ 侧一致（被 test_main 调起前手工跑一次）。"""
    assert show("\n\r\t\v\b\f") == "\\n\\r\\t\\v\\b\\f"
    assert show("\\") == "\\\\"
    assert show("\x01") == "\\x01"
    assert show("\x7f") == "\\x7F"
    assert show("中") == "中"
    return 0


if __name__ == "__main__":
    sys.exit(_self_check())
