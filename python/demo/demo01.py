# ---------------------------------------------------------------------------
# demo01.py —— Nevext Python 实现的示例程序。
#
# 运行：
#   python python/demo/demo01.py              解析内置示例
#   python python/demo/demo01.py 某个文件.txt  解析该文件（UTF-8）
#
# 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此五个实现
# （C++ / GDScript / JavaScript / C# / Python）的 demo 输出可以互相 diff，
# 见 docs/zh-cn/test-plan.md 的「demo 门禁」。
#
# 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
# ---------------------------------------------------------------------------

import os
import sys

# 直接以脚本方式运行时，把 ../nevext 加入搜索路径（库本体是单文件，无安装步骤）。
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "nevext"))

from nevext import Lexer, Parser  # noqa: E402

# 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
BUILTIN = "\n".join([
    "  // 单行注释",
    '  hello "world";',
    "  /*            ",
    "  多行注释      ",
    "  */            ",
    "  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾",
])


def read_input():
    """无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 None。"""
    if len(sys.argv) < 2:
        return BUILTIN
    path = sys.argv[1]
    try:
        # utf-8-sig 顺带吃掉可能存在的 UTF-8 BOM，与其他实现口径一致
        with open(path, "r", encoding="utf-8-sig") as f:
            return f.read()
    except OSError:
        # 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
        print("cannot open: " + path, file=sys.stderr)
        return None


def main():
    # 输出含中文：固定 UTF-8，避免 Windows 控制台默认代码页把内容写坏。
    try:
        sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    except (AttributeError, ValueError):
        pass

    src = read_input()
    if src is None:
        return 2

    lexer = Lexer(src)
    parser = Parser(lexer)
    res = parser.parse_program()

    if res.err:
        # 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
        print("==== Lexer errors =============================")
        for e in lexer.errors:
            print(e)
        print("==== Parser errors =============================")
        for e in parser.errors:
            print(e)
        return 1

    print("statements num: " + str(len(res.val)))
    for i, stmt in enumerate(res.val):
        print("statement " + str(i + 1) + " at line " + str(stmt.line) + ":")
        # 与 C++ 一致：每个串后跟一个空格
        print("  " + "".join(s + " " for s in stmt.strings))
    return 0


if __name__ == "__main__":
    sys.exit(main())
