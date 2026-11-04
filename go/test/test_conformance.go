// ----------------------------------------------------------------------------
// test_conformance.go —— 跨实现通用语料测试（55 条，A01 ~ H04）
//
// 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
// cpp/test/test_conformance.cpp，Python 侧见 python/test/test_conformance.py，
// 其余实现同构。各实现必须完全一致：ID、输入、pause 开关、期望的返回值/
// 产物/错误序列。本文件只做「驱动」，不含任何期望值推导。
//
// 输入与期望串一律用 Go 转义书写：\n \r \t \v \b \f \\ \" \'，非 ASCII 一律
// 写 \uXXXX / \UXXXXXXXX，与文档「避免源文件编码差异」的约定一致。
// 注意 H02：Go 的 string 是 UTF-8 字节序列，emoji 写成 \U0001F600（合法
// 转义，运行期落成 4 字节 UTF-8）；C++ / JS / C# 内部是 UTF-16 码元。
// 两者是同一字符，通用语料只断言内容相等（见文档 X2）。
// ----------------------------------------------------------------------------

package main

import (
	"fmt"
	"strconv"
	"strings"

	nevext "nevext/nevext"
)

// ------------------------------------------------------------------ 期望值表

type stmtSpec struct {
	line    int
	strings []string
}

type errSpec struct {
	line int
	kind string
}

type caseSpec struct {
	id    string
	src   string
	ok    bool
	pause bool
	prog  []stmtSpec
	errs  []errSpec
}

func st(line int, ss ...string) stmtSpec { return stmtSpec{line, ss} }

func er(line int, kind string) errSpec { return errSpec{line, kind} }

var cases = []caseSpec{
	// ---- A 组 程序骨架与语句切分
	{"A01", "", true, true, nil, nil},
	{"A02", "abc", true, true, []stmtSpec{st(1, "abc")}, nil},
	{"A03", "a, b\tc", true, true, []stmtSpec{st(1, "a", "b", "c")}, nil},
	{"A04", "a b; c d; e", true, true, []stmtSpec{st(1, "a", "b"), st(1, "c", "d"), st(1, "e")}, nil},
	{"A05", "a; b", true, true, []stmtSpec{st(1, "a"), st(1, "b")}, nil},
	{"A06", ";;;a;", true, true, []stmtSpec{st(1, "a")}, nil},
	{"A07", "a,,b", true, true, []stmtSpec{st(1, "a", "b")}, nil},
	{"A08", "  \t\n ", true, true, nil, nil},
	{"A09", "a b\nc d;", true, true, []stmtSpec{st(1, "a", "b", "c", "d")}, nil},
	{"A10", "windowSize 1280, 720;", true, true, []stmtSpec{st(1, "windowSize", "1280", "720")}, nil},
	// ---- B 组 引号串
	{"B01", "\"hello world\" x", true, true, []stmtSpec{st(1, "hello world", "x")}, nil},
	{"B02", "'abc'", true, true, []stmtSpec{st(1, "abc")}, nil},
	{"B03", "\"\" z", true, true, []stmtSpec{st(1, "", "z")}, nil},
	{"B04", "'a' \"b\" c", true, true, []stmtSpec{st(1, "a", "b", "c")}, nil},
	{"B05", "'\"' \"'\"", true, true, []stmtSpec{st(1, "\"", "'")}, nil},
	{"B06", "\"a\"b", true, true, []stmtSpec{st(1, "a", "b")}, nil},
	// ---- C 组 转义（只在语法层对 QuotedStr 展开）
	{"C01", "\"a\\nb\"", true, true, []stmtSpec{st(1, "a\nb")}, nil},
	{"C02", "\"\\n\\r\\t\\v\\b\\f\"", true, true, []stmtSpec{st(1, "\n\r\t\v\b\f")}, nil},
	{"C03", "\"a\\\\\"", true, true, []stmtSpec{st(1, "a\\")}, nil},
	{"C04", "\"ab\\\"cd\"", true, true, []stmtSpec{st(1, "ab\"cd")}, nil},
	{"C05", "'a\\qb'", false, true, []stmtSpec{st(1, "a")}, []errSpec{er(1, "InvalidEscape")}},
	{"C06", "\"a\\qb\"; c", false, true, []stmtSpec{st(1, "a")}, []errSpec{er(1, "InvalidEscape")}},
	{"C07", "\"a\\qb\"; c", false, false, []stmtSpec{st(1, "a"), st(1, "c")}, []errSpec{er(1, "InvalidEscape")}},
	// X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
	{"C08", "\"a\\\nb\"", false, true, []stmtSpec{st(1, "a")}, []errSpec{er(1, "InvalidEscape")}},
	{"C09", "\"x\\ny\\tz\"", true, true, []stmtSpec{st(1, "x\ny\tz")}, nil},
	// ---- D 组 裸串边界（裸串不展开转义）
	{"D01", "a\\b", true, true, []stmtSpec{st(1, "a\\b")}, nil},
	{"D02", "a\\nb", true, true, []stmtSpec{st(1, "a\\nb")}, nil},
	{"D03", "a\\qb", true, true, []stmtSpec{st(1, "a\\qb")}, nil},
	{"D04", "a\"b", false, true, []stmtSpec{st(1, "a")},
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	{"D05", "a\"b", false, false, []stmtSpec{st(1, "a"), st(1, "b")},
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	{"D06", "a'b", false, false, []stmtSpec{st(1, "a"), st(1, "b")},
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	{"D07", "a//comment", true, true, []stmtSpec{st(1, "a")}, nil},
	{"D08", "a/**/ b", true, true, []stmtSpec{st(1, "a", "b")}, nil},
	{"D09", "\"a//b\"", true, true, []stmtSpec{st(1, "a//b")}, nil},
	// 退格不是裸串终止字符：整串是一个 RawStr
	{"D10", "a\bb", true, false, []stmtSpec{st(1, "a\bb")}, nil},
	// ---- E 组 注释（未闭合的多行注释不是错误）
	{"E01", "// only comment", true, true, nil, nil},
	{"E02", "/* c1\nc2 */ d", true, true, []stmtSpec{st(2, "d")}, nil},
	{"E03", "a /* xx", true, true, []stmtSpec{st(1, "a")}, nil},
	{"E04", "a /* x\nb; c;", true, true, []stmtSpec{st(1, "a")}, nil},
	{"E05", "/**/a", true, true, []stmtSpec{st(1, "a")}, nil},
	{"E06", "a //c1\n/* c2 */ b", true, true, []stmtSpec{st(1, "a", "b")}, nil},
	// ---- F 组 行号
	{"F01", "a;\nb;\nc;", true, true, []stmtSpec{st(1, "a"), st(2, "b"), st(3, "c")}, nil},
	{"F02", "a;\r\nb;", true, true, []stmtSpec{st(1, "a"), st(2, "b")}, nil},
	{"F03", "\"a\nb\"; c;", true, true, []stmtSpec{st(1, "a\nb"), st(2, "c")}, nil},
	{"F04", "\"a\\\nb\"; c;", false, true, []stmtSpec{st(1, "a")}, []errSpec{er(1, "InvalidEscape")}},
	// 词法错误报在串结束行，语法错误报在串起始行
	{"F05", "a \"b\nc", false, true, []stmtSpec{st(1, "a")},
		[]errSpec{er(2, "UnclosedString"), er(1, "UnexpectedToken")}},
	// ---- G 组 错误与错误恢复
	{"G01", "a \"bc", false, true, []stmtSpec{st(1, "a")},
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	{"G02", "a \"bc", false, false, []stmtSpec{st(1, "a"), st(1, "bc")},
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	// 同一行的多处错误都要记录
	{"G03", "\"a\\qb\" \"c\\qd\"; e", false, false, []stmtSpec{st(1, "a", "c"), st(1, "e")},
		[]errSpec{er(1, "InvalidEscape"), er(1, "InvalidEscape")}},
	{"G04", "\"abc", false, true, nil,
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	{"G05", "\"a\nb", false, true, nil,
		[]errSpec{er(2, "UnclosedString"), er(1, "UnexpectedToken")}},
	// ---- H 组 编码与其它
	{"H01", "a;\"b\";c", true, true, []stmtSpec{st(1, "a"), st(1, "b"), st(1, "c")}, nil},
	// 非 BMP 字符：Go 用 \U0001F600 通用转义写成 4 字节 UTF-8；C++ / JS / C# 内部是代理对
	{"H02", "\"\U0001F600\" x", true, true, []stmtSpec{st(1, "\U0001F600", "x")}, nil},
	{"H03", "\"", false, true, nil,
		[]errSpec{er(1, "UnclosedString"), er(1, "UnexpectedToken")}},
	// 同样为了避免源文件编码差异，非 ASCII 一律写 \uXXXX
	{"H04", "\u6d77\u4e0a\u751f\u660e\u6708 \u5929\u6daf\u5171\u6b64\u65f6", true, true,
		[]stmtSpec{st(1, "\u6d77\u4e0a\u751f\u660e\u6708", "\u5929\u6daf\u5171\u6b64\u65f6")}, nil},
}

// ------------------------------------------------------------------ 驱动逻辑

type runOut struct {
	result      nevext.Result
	prog        []nevext.Statement
	lexErrors   []string
	parseErrors []string
}

// run 一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。
func run(src string, pauseWhenError bool) runOut {
	lexer := nevext.NewLexer(src)
	parser := nevext.NewParser(lexer)
	res := parser.ParseProgram(pauseWhenError)
	return runOut{res.Err, res.Val, lexer.Errors, parser.Errors}
}

// semErrors 错误消息 -> (行号, 种类) 语义序列。词法错误在前，语法错误在后。
func semErrors(out runOut) []string {
	kindOf := func(msg string) string {
		switch {
		case strings.Contains(msg, "unclosed string literal"):
			return "UnclosedString"
		case strings.Contains(msg, "invalid escape character"):
			return "InvalidEscape"
		case strings.Contains(msg, "unexpected token"):
			return "UnexpectedToken"
		}
		return "RAW(" + msg + ")"
	}
	lineOf := func(msg string) int {
		pos := strings.Index(msg, "line ")
		if pos < 0 {
			return -1
		}
		digits := ""
		for _, ch := range msg[pos+5:] {
			if ch < '0' || ch > '9' {
				break
			}
			digits += string(ch)
		}
		n, _ := strconv.Atoi(digits)
		if digits == "" {
			return -1
		}
		return n
	}
	all := append(append([]string{}, out.lexErrors...), out.parseErrors...)
	res := make([]string, 0, len(all))
	for _, e := range all {
		res = append(res, strconv.Itoa(lineOf(e))+":"+kindOf(e))
	}
	return res
}

func findCase(id string) *caseSpec {
	for i := range cases {
		if cases[i].id == id {
			return &cases[i]
		}
	}
	return nil
}

func checkCase(ctx *Context, caseID string) {
	want := findCase(caseID)
	if want == nil {
		ctx.Failures++
		fmt.Println("    [FAIL] 语料里没有这条用例: " + caseID)
		return
	}

	out := run(want.src, want.pause)

	// 1) 返回值
	wantResult := "Error"
	if want.ok {
		wantResult = "Success"
	}
	gotResult := "Error"
	if out.result == nevext.Success {
		gotResult = "Success"
	}
	checkEq(ctx, wantResult, gotResult, "返回值")

	// 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
	checkEq(ctx, len(want.prog), len(out.prog), "语句数")
	for i := 0; i < len(out.prog) && i < len(want.prog); i++ {
		checkEq(ctx, want.prog[i].line, out.prog[i].Line, "语句["+strconv.Itoa(i)+"]行号")
		checkEq(ctx, len(want.prog[i].strings), len(out.prog[i].Strings),
			"语句["+strconv.Itoa(i)+"]字符串数")
		for j := 0; j < len(out.prog[i].Strings) && j < len(want.prog[i].strings); j++ {
			checkEq(ctx, want.prog[i].strings[j], out.prog[i].Strings[j],
				"语句["+strconv.Itoa(i)+"]字符串["+strconv.Itoa(j)+"]")
		}
	}

	// 3) 错误：按 (行号,种类) 序列比较，顺序敏感
	got := semErrors(out)
	exp := make([]string, 0, len(want.errs))
	for _, e := range want.errs {
		exp = append(exp, strconv.Itoa(e.line)+":"+e.kind)
	}
	checkEq(ctx, len(exp), len(got), "错误数")
	for i := 0; i < len(got) && i < len(exp); i++ {
		checkEq(ctx, exp[i], got[i], "错误["+strconv.Itoa(i)+"]")
	}
}

// quoteDump dump 用：show 已把 \ 转成 \\；在其之后再转义引号才是安全的。
func quoteDump(s string) string {
	return strings.ReplaceAll(s, "\"", "\\\"")
}

// dumpConformance 按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。
func dumpConformance() {
	for _, c := range cases {
		out := run(c.src, c.pause)
		var parts []string
		for _, stmt := range out.prog {
			quoted := make([]string, 0, len(stmt.Strings))
			for _, s := range stmt.Strings {
				quoted = append(quoted, "\""+quoteDump(show(s))+"\"")
			}
			parts = append(parts, strconv.Itoa(stmt.Line)+":["+strings.Join(quoted, ",")+"]")
		}
		prog := strings.Join(parts, "|")
		errs := strings.Join(semErrors(out), ",")
		ok := "Err"
		if out.result == nevext.Success {
			ok = "OK"
		}
		pause := "0"
		if c.pause {
			pause = "1"
		}
		fmt.Println(c.id + "|" + ok + "|" + prog + "|" + errs + "|pause=" + pause)
	}
}

// ------------------------------------------------------------------ 用例注册
// 每条语料一个独立用例名，支持 go run ./test Conformance.C0 这样的前缀筛选。

func registerAllConformance() {
	for i := range cases {
		c := &cases[i]
		register("Conformance."+c.id, func(ctx *Context) { checkCase(ctx, c.id) })
	}
}
