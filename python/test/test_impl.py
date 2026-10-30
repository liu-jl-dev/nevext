# ---------------------------------------------------------------------------
# test_impl.py —— Python 实现私有的用例（与 JavaScript 侧 test_impl.js 对应）
#
# 通用语料（test_conformance.py）各实现共用；本文件放 Python 自己关心的
# 工程性用例：对象复用、终止性、大输入，以及一条码点模型用例。
# ---------------------------------------------------------------------------

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "nevext"))

import nevext  # noqa: E402

from test_framework import check_eq, check_true, register  # noqa: E402

Lexer = nevext.Lexer
Parser = nevext.Parser
Result = nevext.Result
TokenType = nevext.TokenType


# PY-01 Lexer.reset / Parser.reset 能让实例重用到另一份源码
#       （与 GDScript 的 IMP-02、C++ 的 LEX-26 / PAR-18、JS 的 JS-01 对应）
def reset_are_reusable(ctx):
    lexer = Lexer("a b")
    parser = Parser(lexer)
    first = parser.parse_program(True)
    check_eq(ctx, 1, len(first.val), "第一次语句数")
    check_eq(ctx, "Success", "Success" if first.err == Result.Success else "Error", "第一次返回值")

    lexer.reset("c; d")
    parser.reset(lexer)
    second = parser.parse_program(True)
    check_eq(ctx, 2, len(second.val), "第二次语句数")
    if len(second.val) >= 2:
        check_eq(ctx, "c", second.val[0].strings[0], "第 1 条语句")
        check_eq(ctx, "d", second.val[1].strings[0], "第 2 条语句")
    check_eq(ctx, 0, len(parser.errors), "Parser 错误数（reset 应清空）")
    check_eq(ctx, 0, len(lexer.errors), "Lexer 错误数（reset 应清空）")
    check_eq(ctx, 1, lexer.line, "行号应回到 1")


# PY-02 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
def pathological_inputs_terminate(ctx):
    src = "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;"
    for i in range(len(src) + 1):
        lexer = Lexer(src[:i])
        reached_eof = False
        for _ in range(200):
            if lexer.next_token().type == TokenType.EndOfFile:
                reached_eof = True
                break
        check_true(ctx, reached_eof, "前缀[" + str(i) + "]能在阈值内扫到 EOF")


# PY-03 长裸串不被截断
def long_raw_token_is_complete(ctx):
    src = "x" * 1000 + ";"
    lexer = Lexer(src)
    token = lexer.next_token()
    check_eq(ctx, "RawStr", "RawStr" if token.type == TokenType.RawStr else "其它", "token 类型")
    check_eq(ctx, 1000, len(token.literal), "裸串长度")


# PY-04 大量语句全部解析（游标实现不应有码元级性能悬崖）
def many_statements_are_all_parsed(ctx):
    src = "\n".join("s" + str(i) + " a b;" for i in range(500))
    lexer = Lexer(src)
    parser = Parser(lexer)
    res = parser.parse_program(True)
    check_eq(ctx, 500, len(res.val), "语句数")
    check_eq(ctx, "Success", "Success" if res.err == Result.Success else "Error", "返回值")
    check_eq(ctx, "s0", res.val[0].strings[0], "首语句首串")
    check_eq(ctx, "s499", res.val[499].strings[0], "末语句首串")
    check_eq(ctx, 500, res.val[499].line, "末语句行号")


# PY-05 孤立代理项按码点原样保留（Python 的 str 允许孤立代理项；
#       另三套实现按 UTF-16 码元保留，语义相同）
def lone_surrogate_preserved(ctx):
    lexer = Lexer("x \ud800 y \"\udfff\"")
    parser = Parser(lexer)
    res = parser.parse_program(True)
    check_eq(ctx, 1, len(res.val), "语句数")
    if len(res.val) >= 1:
        strings = res.val[0].strings
        check_eq(ctx, 4, len(strings), "字符串数")
        if len(strings) >= 4:
            check_eq(ctx, "\ud800", strings[1], "孤立高代理项原样保留")
            check_eq(ctx, "\udfff", strings[3], "孤立低代理项经语法层原样保留")
    check_eq(ctx, 0, len(lexer.errors) + len(parser.errors), "错误数")


def register_all():
    register("Impl.ResetAreReusable", reset_are_reusable)
    register("Impl.PathologicalInputsTerminate", pathological_inputs_terminate)
    register("Impl.LongRawTokenIsComplete", long_raw_token_is_complete)
    register("Impl.ManyStatementsAreAllParsed", many_statements_are_all_parsed)
    register("Impl.LoneSurrogatePreserved", lone_surrogate_preserved)
