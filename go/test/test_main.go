// ----------------------------------------------------------------------------
// test_main.go —— 测试入口。
//
// 用法（与各实现测试入口的约定对齐）：
//   go run ./test            跑全部用例（Conformance + Impl）
//   ... Conformance.C0       只跑名字含该子串的用例
//   ... -v                   详细模式（保留参数兼容）
//   ... --dump               按 C++ 侧同一行格式打印 55 条语料的实际结果，
//                            用于跨实现 diff 门禁
// ----------------------------------------------------------------------------

package main

import "os"

func main() {
	registerAllConformance()
	registerAllImpl()

	args := os.Args[1:]
	for _, a := range args {
		if a == "--dump" {
			dumpConformance()
			os.Exit(0)
		}
	}
	os.Exit(runAll(args))
}
