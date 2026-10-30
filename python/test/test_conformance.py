# ---------------------------------------------------------------------------
# test_conformance.py —— 跨实现通用语料测试（55 条，A01 ~ H04）
#
# 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
# cpp/test/test_conformance.cpp，GDScript 侧见 gdscript/test/test_conformance.gd，
# JavaScript 侧见 nodejs/test/test_conformance.js，C# 侧见 csharp/test/TestConformance.cs。
# 各实现必须完全一致：ID、输入、pause 开关、期望的返回值/产物/错误序列。
# 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。
#
# 输入与期望串一律用 Python 转义书写：\n \r \t \v \b \f \\ \" \'，非 ASCII 一律
# 写 \uXXXX / \UXXXXXXXX 码点，与文档「避免源文件编码差异」的约定一致。
# 注意 H02：Python 的 str 是码点序列，emoji 写成一个码点 \U0001F600；
# 另三套实现内部是 UTF-16 码元，用代理对表示。两者是同一字符，
# 通用语料只断言内容相等（见文档 X2）。
# ---------------------------------------------------------------------------

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "nevext"))

from collections import namedtuple  # noqa: E402

import nevext  # noqa: E402

from test_framework import check_eq, register, show  # noqa: E402

Lexer = nevext.Lexer
Parser = nevext.Parser
Result = nevext.Result

# ------------------------------------------------------------------ 期望值表

Stmt = namedtuple("Stmt", "line strings")
Err = namedtuple("Err", "line kind")
Case = namedtuple("Case", "id src ok pause prog errs")


def St(line, *strings):
    return Stmt(line, list(strings))


def Er(line, kind):
    return Err(line, kind)


CASES = [
    # ---- A 组 程序骨架与语句切分
    Case("A01", "", True, True, [], []),
    Case("A02", "abc", True, True, [St(1, "abc")], []),
    Case("A03", "a, b\tc", True, True, [St(1, "a", "b", "c")], []),
    Case("A04", "a b; c d; e", True, True, [St(1, "a", "b"), St(1, "c", "d"), St(1, "e")], []),
    Case("A05", "a; b", True, True, [St(1, "a"), St(1, "b")], []),
    Case("A06", ";;;a;", True, True, [St(1, "a")], []),
    Case("A07", "a,,b", True, True, [St(1, "a", "b")], []),
    Case("A08", "  \t\n ", True, True, [], []),
    Case("A09", "a b\nc d;", True, True, [St(1, "a", "b", "c", "d")], []),
    Case("A10", "windowSize 1280, 720;", True, True, [St(1, "windowSize", "1280", "720")], []),
    # ---- B 组 引号串
    Case("B01", '"hello world" x', True, True, [St(1, "hello world", "x")], []),
    Case("B02", "'abc'", True, True, [St(1, "abc")], []),
    Case("B03", '"" z', True, True, [St(1, "", "z")], []),
    Case("B04", "'a' \"b\" c", True, True, [St(1, "a", "b", "c")], []),
    Case("B05", "'\"' \"'\"", True, True, [St(1, '"', "'")], []),
    Case("B06", '"a"b', True, True, [St(1, "a", "b")], []),
    # ---- C 组 转义（只在语法层对 QuotedStr 展开）
    Case("C01", '"a\\nb"', True, True, [St(1, "a\nb")], []),
    Case("C02", '"\\n\\r\\t\\v\\b\\f"', True, True, [St(1, "\n\r\t\v\b\f")], []),
    Case("C03", '"a\\\\"', True, True, [St(1, "a\\")], []),
    Case("C04", '"ab\\"cd"', True, True, [St(1, 'ab"cd')], []),
    Case("C05", "'a\\qb'", False, True, [St(1, "a")], [Er(1, "InvalidEscape")]),
    Case("C06", '"a\\qb"; c', False, True, [St(1, "a")], [Er(1, "InvalidEscape")]),
    Case("C07", '"a\\qb"; c', False, False, [St(1, "a"), St(1, "c")], [Er(1, "InvalidEscape")]),
    # X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
    Case("C08", '"a\\\nb"', False, True, [St(1, "a")], [Er(1, "InvalidEscape")]),
    Case("C09", '"x\\ny\\tz"', True, True, [St(1, "x\ny\tz")], []),
    # ---- D 组 裸串边界（裸串不展开转义）
    Case("D01", "a\\b", True, True, [St(1, "a\\b")], []),
    Case("D02", "a\\nb", True, True, [St(1, "a\\nb")], []),
    Case("D03", "a\\qb", True, True, [St(1, "a\\qb")], []),
    Case("D04", 'a"b', False, True, [St(1, "a")],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    Case("D05", 'a"b', False, False, [St(1, "a"), St(1, "b")],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    Case("D06", "a'b", False, False, [St(1, "a"), St(1, "b")],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    Case("D07", "a//comment", True, True, [St(1, "a")], []),
    Case("D08", "a/**/ b", True, True, [St(1, "a", "b")], []),
    Case("D09", '"a//b"', True, True, [St(1, "a//b")], []),
    # 退格不是裸串终止字符：整串是一个 RawStr
    Case("D10", "a\bb", True, False, [St(1, "a\bb")], []),
    # ---- E 组 注释（未闭合的多行注释不是错误）
    Case("E01", "// only comment", True, True, [], []),
    Case("E02", "/* c1\nc2 */ d", True, True, [St(2, "d")], []),
    Case("E03", "a /* xx", True, True, [St(1, "a")], []),
    Case("E04", "a /* x\nb; c;", True, True, [St(1, "a")], []),
    Case("E05", "/**/a", True, True, [St(1, "a")], []),
    Case("E06", "a //c1\n/* c2 */ b", True, True, [St(1, "a", "b")], []),
    # ---- F 组 行号
    Case("F01", "a;\nb;\nc;", True, True, [St(1, "a"), St(2, "b"), St(3, "c")], []),
    Case("F02", "a;\r\nb;", True, True, [St(1, "a"), St(2, "b")], []),
    Case("F03", '"a\nb"; c;', True, True, [St(1, "a\nb"), St(2, "c")], []),
    Case("F04", '"a\\\nb"; c;', False, True, [St(1, "a")], [Er(1, "InvalidEscape")]),
    # 词法错误报在串结束行，语法错误报在串起始行
    Case("F05", 'a "b\nc', False, True, [St(1, "a")],
         [Er(2, "UnclosedString"), Er(1, "UnexpectedToken")]),
    # ---- G 组 错误与错误恢复
    Case("G01", 'a "bc', False, True, [St(1, "a")],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    Case("G02", 'a "bc', False, False, [St(1, "a"), St(1, "bc")],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    # 同一行的多处错误都要记录
    Case("G03", '"a\\qb" "c\\qd"; e', False, False, [St(1, "a", "c"), St(1, "e")],
         [Er(1, "InvalidEscape"), Er(1, "InvalidEscape")]),
    Case("G04", '"abc', False, True, [],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    Case("G05", '"a\nb', False, True, [],
         [Er(2, "UnclosedString"), Er(1, "UnexpectedToken")]),
    # ---- H 组 编码与其它
    Case("H01", 'a;"b";c', True, True, [St(1, "a"), St(1, "b"), St(1, "c")], []),
    # 非 BMP 字符：Python 写作一个码点；C++ / JS / C# 内部是 UTF-16 代理对
    Case("H02", '"\U0001F600" x', True, True, [St(1, "\U0001F600", "x")], []),
    Case("H03", '"', False, True, [],
         [Er(1, "UnclosedString"), Er(1, "UnexpectedToken")]),
    # 同样为了避免源文件编码差异，非 ASCII 一律写 \uXXXX 码点
    Case("H04", "\u6d77\u4e0a\u751f\u660e\u6708 \u5929\u6daf\u5171\u6b64\u65f6", True, True,
         [St(1, "\u6d77\u4e0a\u751f\u660e\u6708", "\u5929\u6daf\u5171\u6b64\u65f6")], []),
]

# ------------------------------------------------------------------ 驱动逻辑


def run(src, pause_when_error):
    """一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。"""
    lexer = Lexer(src)
    parser = Parser(lexer)
    res = parser.parse_program(pause_when_error)
    return {
        "result": res.err,
        "prog": res.val,
        "lex_errors": lexer.errors,
        "parse_errors": parser.errors,
    }


def sem_errors(out):
    """错误消息 -> (行号, 种类) 语义序列。词法错误在前，语法错误在后。"""

    def kind_of(msg):
        if "unclosed string literal" in msg:
            return "UnclosedString"
        if "invalid escape character" in msg:
            return "InvalidEscape"
        if "unexpected token" in msg:
            return "UnexpectedToken"
        return "RAW(" + msg + ")"

    def line_of(msg):
        pos = msg.find("line ")
        if pos < 0:
            return -1
        digits = ""
        for ch in msg[pos + 5 :]:
            if not ch.isdigit():
                break
            digits += ch
        return int(digits) if digits else -1

    return [str(line_of(e)) + ":" + kind_of(e) for e in out["lex_errors"] + out["parse_errors"]]


def find_case(case_id):
    for c in CASES:
        if c.id == case_id:
            return c
    return None


def check_case(ctx, case_id):
    want = find_case(case_id)
    if want is None:
        ctx.failures += 1
        print("    [FAIL] 语料里没有这条用例: " + case_id)
        return

    out = run(want.src, want.pause)

    # 1) 返回值
    check_eq(ctx, "Success" if want.ok else "Error",
             "Success" if out["result"] == Result.Success else "Error", "返回值")

    # 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
    check_eq(ctx, len(want.prog), len(out["prog"]), "语句数")
    for i in range(min(len(out["prog"]), len(want.prog))):
        check_eq(ctx, want.prog[i].line, out["prog"][i].line, "语句[" + str(i) + "]行号")
        check_eq(ctx, len(want.prog[i].strings), len(out["prog"][i].strings),
                 "语句[" + str(i) + "]字符串数")
        for j in range(min(len(out["prog"][i].strings), len(want.prog[i].strings))):
            check_eq(ctx, want.prog[i].strings[j], out["prog"][i].strings[j],
                     "语句[" + str(i) + "]字符串[" + str(j) + "]")

    # 3) 错误：按 (行号,种类) 序列比较，顺序敏感
    got = sem_errors(out)
    exp = [str(e.line) + ":" + e.kind for e in want.errs]
    check_eq(ctx, len(exp), len(got), "错误数")
    for i in range(min(len(got), len(exp))):
        check_eq(ctx, exp[i], got[i], "错误[" + str(i) + "]")


def quote_dump(s):
    """dump 用：show 已把 \\ 转成 \\\\；在其之后再转义引号才是安全的。"""
    return s.replace('"', '\\"')


def dump_conformance():
    """按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。"""
    for c in CASES:
        out = run(c.src, c.pause)
        parts = []
        for stmt in out["prog"]:
            quoted = ",".join('"' + quote_dump(show(s)) + '"' for s in stmt.strings)
            parts.append(str(stmt.line) + ":[" + quoted + "]")
        prog = "|".join(parts)
        errs = ",".join(sem_errors(out))
        ok = "OK" if out["result"] == Result.Success else "Err"
        print(c.id + "|" + ok + "|" + prog + "|" + errs + "|pause=" + ("1" if c.pause else "0"))


# ------------------------------------------------------------------ 用例注册
# 每条语料一个独立用例名，支持 test_main.py Conformance.C0 这样的前缀筛选。

def register_all():
    for c in CASES:
        register("Conformance." + c.id, lambda ctx, cid=c.id: check_case(ctx, cid))
