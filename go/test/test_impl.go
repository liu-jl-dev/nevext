// ----------------------------------------------------------------------------
// test_impl.go —— Go 实现私有的用例（与 Python 侧 test_impl.py 对应）
//
// 通用语料（test_conformance.go）各实现共用；本文件放 Go 自己关心的
// 工程性用例：对象复用、终止性、长裸串、大输入，以及一条字节模型用例。
// ----------------------------------------------------------------------------

package main

import (
	"strconv"
	"strings"

	nevext "nevext/nevext"
)

// GO-01 Lexer.Reset / Parser.Reset 能让实例重用到另一份源码
//
//	（与 C++ 的 LEX-26 / PAR-18、Python 的 PY-01 对应）
func resetAreReusable(ctx *Context) {
	lexer := nevext.NewLexer("a b")
	parser := nevext.NewParser(lexer)
	first := parser.ParseProgram(true)
	checkEq(ctx, 1, len(first.Val), "第一次语句数")
	checkEq(ctx, "Success", resultName(first.Err), "第一次返回值")

	lexer.Reset("c; d")
	parser.Reset(lexer)
	second := parser.ParseProgram(true)
	checkEq(ctx, 2, len(second.Val), "第二次语句数")
	if len(second.Val) >= 2 {
		checkEq(ctx, "c", second.Val[0].Strings[0], "第 1 条语句")
		checkEq(ctx, "d", second.Val[1].Strings[0], "第 2 条语句")
	}
	checkEq(ctx, 0, len(parser.Errors), "Parser 错误数（reset 应清空）")
	checkEq(ctx, 0, len(lexer.Errors), "Lexer 错误数（reset 应清空）")
	checkEq(ctx, 1, lexer.Line, "行号应回到 1")
}

// GO-02 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
func pathologicalInputsTerminate(ctx *Context) {
	src := "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;"
	for i := 0; i <= len(src); i++ {
		lexer := nevext.NewLexer(src[:i])
		reachedEOF := false
		for n := 0; n < 200; n++ {
			if lexer.NextToken().Type == nevext.EndOfFile {
				reachedEOF = true
				break
			}
		}
		checkTrue(ctx, reachedEOF, "前缀["+strconv.Itoa(i)+"]能在阈值内扫到 EOF")
	}
}

// GO-03 长裸串不被截断
func longRawTokenIsComplete(ctx *Context) {
	src := strings.Repeat("x", 1000) + ";"
	lexer := nevext.NewLexer(src)
	token := lexer.NextToken()
	checkEq(ctx, "RawStr", tokenTypeName(token.Type), "token 类型")
	checkEq(ctx, 1000, len(token.Literal), "裸串长度")
}

// GO-04 大量语句全部解析（游标实现不应有性能悬崖）
func manyStatementsAreAllParsed(ctx *Context) {
	var b strings.Builder
	for i := 0; i < 500; i++ {
		b.WriteString("s" + strconv.Itoa(i) + " a b;\n")
	}
	lexer := nevext.NewLexer(b.String())
	parser := nevext.NewParser(lexer)
	res := parser.ParseProgram(true)
	checkEq(ctx, 500, len(res.Val), "语句数")
	checkEq(ctx, "Success", resultName(res.Err), "返回值")
	checkEq(ctx, "s0", res.Val[0].Strings[0], "首语句首串")
	checkEq(ctx, "s499", res.Val[499].Strings[0], "末语句首串")
	checkEq(ctx, 500, res.Val[499].Line, "末语句行号")
}

// GO-05 非 UTF-8 的孤立高位字节原样保留（Go string 是字节序列，可以容纳
// 任意字节；Rust / Python 的 str 不行。缺口 GO1：元素计数按字节）
func arbitraryBytesPreserved(ctx *Context) {
	lexer := nevext.NewLexer("x \xc3 y")
	parser := nevext.NewParser(lexer)
	res := parser.ParseProgram(true)
	checkEq(ctx, 1, len(res.Val), "语句数")
	if len(res.Val) >= 1 {
		strs := res.Val[0].Strings
		checkEq(ctx, 3, len(strs), "字符串数")
		if len(strs) >= 3 {
			checkEq(ctx, "\xc3", strs[1], "孤立高位字节原样保留")
			checkEq(ctx, "y", strs[2], "后续裸串不受影响")
		}
	}
	checkEq(ctx, 0, len(lexer.Errors)+len(parser.Errors), "错误数")
}

func resultName(r nevext.Result) string {
	if r == nevext.Success {
		return "Success"
	}
	return "Error"
}

func tokenTypeName(t nevext.TokenType) string {
	if t == nevext.RawStr {
		return "RawStr"
	}
	return "其它"
}

func registerAllImpl() {
	register("Impl.ResetAreReusable", resetAreReusable)
	register("Impl.PathologicalInputsTerminate", pathologicalInputsTerminate)
	register("Impl.LongRawTokenIsComplete", longRawTokenIsComplete)
	register("Impl.ManyStatementsAreAllParsed", manyStatementsAreAllParsed)
	register("Impl.ArbitraryBytesPreserved", arbitraryBytesPreserved)
}
