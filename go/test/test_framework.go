// ----------------------------------------------------------------------------
// test_framework.go —— 极简单元测试框架（零第三方依赖）
//
// 设计取舍与 cpp/test/test_framework.hpp 一致：
//   · 不引入任何测试框架（不用 testing 等）：nevext 保持零依赖，
//     go run ./test 一条命令就能跑；
//   · 失败只累加计数并打印，单个断言失败不影响后续用例；
//   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
// ----------------------------------------------------------------------------

package main

import (
	"fmt"
	"strings"
)

// Context 单个用例的运行上下文：累加断言数与失败数。
type Context struct {
	Name     string
	Checks   int
	Failures int
}

type testCase struct {
	name string
	fn   func(*Context)
}

var registry []testCase

func register(name string, fn func(*Context)) {
	registry = append(registry, testCase{name, fn})
}

// show 把控制字符转成可见转义，避免诊断信息把终端刷乱。
// 与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：
// 六个具名控制字符用 \n \r \t \v \b \f，反斜杠翻倍，其余控制字符 \xNN。
// 按字节处理：Go string 是 UTF-8 字节序列，控制字节（<0x20 或 0x7F）不会
// 出现在多字节序列里，逐字节与逐码点结果相同；>=0x80 的字节原样透传。
func show(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		switch c := s[i]; {
		case c == '\n':
			b.WriteString("\\n")
		case c == '\r':
			b.WriteString("\\r")
		case c == '\t':
			b.WriteString("\\t")
		case c == '\v':
			b.WriteString("\\v")
		case c == '\b':
			b.WriteString("\\b")
		case c == '\f':
			b.WriteString("\\f")
		case c == '\\':
			b.WriteString("\\\\")
		case c < 0x20 || c == 0x7F:
			fmt.Fprintf(&b, "\\x%02X", c)
		default:
			b.WriteByte(c)
		}
	}
	return b.String()
}

func quote(v string) string {
	return "\"" + show(v) + "\""
}

func reportFailure(ctx *Context, label, detail string) {
	ctx.Failures++
	fmt.Println("    [FAIL] " + label + "\n           " + detail)
}

func checkTrue(ctx *Context, cond bool, label string) {
	ctx.Checks++
	if !cond {
		reportFailure(ctx, label, "期望为真")
	}
}

func checkFalse(ctx *Context, cond bool, label string) {
	ctx.Checks++
	if cond {
		reportFailure(ctx, label, "期望为假")
	}
}

// checkEq 与 Python 侧 check_eq 同构：字符串走 quote，其余走 fmt.Sprint。
func checkEq(ctx *Context, expected, actual interface{}, label string) {
	ctx.Checks++
	if fmt.Sprint(expected) != fmt.Sprint(actual) {
		expText := fmt.Sprint(expected)
		actText := fmt.Sprint(actual)
		if e, ok := expected.(string); ok {
			expText = quote(e)
		}
		if a, ok := actual.(string); ok {
			actText = quote(a)
		}
		reportFailure(ctx, label, "期望 "+expText+"，实际 "+actText)
	}
}

func runAll(argv []string) int {
	filter := ""
	for _, arg := range argv {
		if arg == "-v" || arg == "--verbose" {
			// 详细模式占位：本框架已逐用例打印，保留参数只为兼容
		} else if arg != "" {
			filter = arg
		}
	}

	caseFailed := 0
	casePassed := 0
	totalChecks := 0
	totalFailures := 0

	fmt.Println("==== nevext Go 单元测试 ====")
	for _, tc := range registry {
		if filter != "" && !strings.Contains(tc.name, filter) {
			continue
		}
		ctx := &Context{Name: tc.name}
		fmt.Println("[ RUN  ] " + tc.name)
		func() {
			// 用例 panic 要计入失败而不是中断整轮（对应 Python 的 try/except）
			defer func() {
				if r := recover(); r != nil {
					ctx.Failures++
					fmt.Printf("    [FAIL] 用例抛出异常: %v\n", r)
				}
			}()
			tc.fn(ctx)
		}()
		totalChecks += ctx.Checks
		totalFailures += ctx.Failures
		if ctx.Failures != 0 {
			caseFailed++
			fmt.Printf("[ FAIL ] %s   失败断言 %d/%d\n", tc.name, ctx.Failures, ctx.Checks)
		} else {
			casePassed++
			fmt.Printf("[  OK  ] %s   %d 个断言通过\n", tc.name, ctx.Checks)
		}
	}

	fmt.Printf("--------------------------------------------------\n"+
		"用例: %d，通过: %d，失败: %d\n"+
		"断言: %d，失败: %d\n", casePassed+caseFailed, casePassed, caseFailed,
		totalChecks, totalFailures)
	if caseFailed != 0 {
		fmt.Println("结果: 失败")
		return 1
	}
	fmt.Println("结果: 全部通过")
	return 0
}

// _selfCheck 框架自检：确保 show 的转义与 C++ 侧一致（手工调起）。
func _selfCheck() int {
	assert := func(cond bool) {
		if !cond {
			panic("self check failed")
		}
	}
	assert(show("\n\r\t\v\b\f") == "\\n\\r\\t\\v\\b\\f")
	assert(show("\\") == "\\\\")
	assert(show("\x01") == "\\x01")
	assert(show("\x7f") == "\\x7F")
	assert(show("中") == "中")
	return 0
}
