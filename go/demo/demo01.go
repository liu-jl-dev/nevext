// ----------------------------------------------------------------------------
// demo01.go —— Nevext Go 实现的示例程序。
//
// 运行：
//   go run ./demo              解析内置示例
//   go run ./demo 某个文件.txt  解析该文件（UTF-8）
//
// 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此所有实现
// （C++ / GDScript / JavaScript / C# / Python / Lua / Go / Rust）的 demo
// 输出可以互相 diff，见 docs/zh-cn/test-plan.md 的「demo 门禁」。
//
// 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
// ----------------------------------------------------------------------------

package main

import (
	"fmt"
	"os"
	"strings"

	nevext "nevext/nevext"
)

// builtin 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
func builtin() string {
	parts := []string{
		"  // 单行注释",
		"  hello \"world\";",
		"  /*            ",
		"  多行注释      ",
		"  */            ",
		"  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾",
	}
	return strings.Join(parts, "\n")
}

// readInput 无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 nil。
func readInput() (string, bool) {
	if len(os.Args) < 2 {
		return builtin(), true
	}
	path := os.Args[1]
	data, err := os.ReadFile(path)
	if err != nil {
		// 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
		fmt.Fprintln(os.Stderr, "cannot open: "+path)
		return "", false
	}
	// utf-8-sig 顺带吃掉可能存在的 UTF-8 BOM，与其他实现口径一致
	text := string(data)
	text = strings.TrimPrefix(text, "\uFEFF")
	return text, true
}

func main() {
	src, ok := readInput()
	if !ok {
		os.Exit(2)
	}

	lexer := nevext.NewLexer(src)
	parser := nevext.NewParser(lexer)
	res := parser.ParseProgram(true)

	if res.Err != nevext.Success {
		// 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
		fmt.Println("==== Lexer errors =============================")
		for _, e := range lexer.Errors {
			fmt.Println(e)
		}
		fmt.Println("==== Parser errors =============================")
		for _, e := range parser.Errors {
			fmt.Println(e)
		}
		os.Exit(1)
	}

	fmt.Println("statements num: " + fmt.Sprint(len(res.Val)))
	for i, stmt := range res.Val {
		fmt.Println("statement " + fmt.Sprint(i+1) + " at line " + fmt.Sprint(stmt.Line) + ":")
		// 与 C++ 一致：每个串后跟一个空格
		var line strings.Builder
		line.WriteString("  ")
		for _, s := range stmt.Strings {
			line.WriteString(s)
			line.WriteString(" ")
		}
		fmt.Println(line.String())
	}
	os.Exit(0)
}
