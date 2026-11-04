// ----------------------------------------------------------------------------
// test_main.rs —— 测试入口。
//
// 用法（与各实现测试入口的约定对齐）：
//   cargo run --bin nevext_test            跑全部用例（Conformance + Impl）
//   ... -- Conformance.C0                  只跑名字含该子串的用例
//   ... -- -v                              详细模式（保留参数兼容）
//   ... -- --dump                          按 C++ 侧同一行格式打印 55 条语料的
//                                          实际结果，用于跨实现 diff 门禁
// ----------------------------------------------------------------------------

mod test_conformance;
mod test_framework;
mod test_impl;

use test_framework::run_all;

fn main() {
    let argv: Vec<String> = std::env::args().skip(1).collect();

    test_conformance::register_all();
    test_impl::register_all();

    if argv.iter().any(|a| a == "--dump") {
        test_conformance::dump_conformance();
        std::process::exit(0);
    }
    std::process::exit(run_all(&argv));
}
