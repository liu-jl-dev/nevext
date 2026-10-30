# ---------------------------------------------------------------------------
# test_main.py —— 测试入口。
#
# 用法（与 cpp/test、gdscript/test、nodejs/test、csharp/test 的入口约定对齐）：
#   python test/test_main.py            跑全部用例（Conformance + Impl）
#   ... Conformance.C0                  只跑名字含该子串的用例
#   ... -v                              详细模式（保留参数兼容）
#   ... --dump                          按 C++ 侧同一行格式打印 55 条语料的
#                                       实际结果，用于跨实现 diff 门禁
# ---------------------------------------------------------------------------

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "..", "nevext"))

import test_conformance  # noqa: E402
import test_impl  # noqa: E402

from test_framework import run_all  # noqa: E402


def main(argv):
    # dump 含中文与非 BMP 字符（H02/H04）；Windows 控制台默认代码页不是 UTF-8。
    # errors="backslashreplace" 让孤立代理项这类无法编码的字符降级成 \udXXX 文本，
    # 保证诊断信息不会因为编码问题把整轮测试打断。
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace", newline="\n")
    except (AttributeError, ValueError):  # 老解释器或已重定向到不支持 reconfigure 的流
        pass

    test_conformance.register_all()
    test_impl.register_all()

    if "--dump" in argv:
        test_conformance.dump_conformance()
        return 0
    return run_all(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
