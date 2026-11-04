// ----------------------------------------------------------------------------
// test_framework.rs —— 极简单元测试框架（零第三方依赖）
//
// 设计取舍与 cpp/test/test_framework.hpp 一致：
//   · 不引入任何测试框架（不用 cargo test 宏体系）：nevext 保持零依赖，
//     cargo run --bin nevext_test 一条命令就能跑；
//   · 失败只累加计数并打印，单个断言失败不影响后续用例；
//   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
// ----------------------------------------------------------------------------

use std::panic::catch_unwind;
use std::sync::{Mutex, OnceLock};

/// 单个用例的运行上下文：累加断言数与失败数。
/// （name 保留是为了与其它实现的 Context 对齐；Python 侧同样存放未读）
pub struct Context {
    #[allow(dead_code)]
    pub name: String,
    pub checks: usize,
    pub failures: usize,
}

type CaseFn = Box<dyn Fn(&mut Context) + Send>;

fn registry() -> &'static Mutex<Vec<(String, CaseFn)>> {
    static REGISTRY: OnceLock<Mutex<Vec<(String, CaseFn)>>> = OnceLock::new();
    REGISTRY.get_or_init(|| Mutex::new(Vec::new()))
}

pub fn register(name: &str, f: impl Fn(&mut Context) + Send + 'static) {
    registry()
        .lock()
        .unwrap()
        .push((name.to_string(), Box::new(f)));
}

/// 把控制字符转成可见转义，避免诊断信息把终端刷乱。
/// 与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：
/// 六个具名控制字符用 \n \r \t \v \b \f，反斜杠翻倍，其余控制字符 \xNN。
/// 按字节处理：Rust &str 是 UTF-8 字节序列，控制字节（<0x20 或 0x7F）不会
/// 出现在多字节序列里，逐字节与逐码点结果相同；>=0x80 的字节原样透传。
pub fn show(s: &str) -> String {
    let mut out: Vec<u8> = Vec::with_capacity(s.len());
    for &b in s.as_bytes() {
        match b {
            b'\n' => out.extend_from_slice(b"\\n"),
            b'\r' => out.extend_from_slice(b"\\r"),
            b'\t' => out.extend_from_slice(b"\\t"),
            0x0b => out.extend_from_slice(b"\\v"),
            0x08 => out.extend_from_slice(b"\\b"),
            0x0c => out.extend_from_slice(b"\\f"),
            b'\\' => out.extend_from_slice(b"\\\\"),
            0x00..=0x1f | 0x7f => out.extend_from_slice(format!("\\x{:02X}", b).as_bytes()),
            _ => out.push(b),
        }
    }
    // 输入是合法 UTF-8，控制字节替换不破坏多字节序列，结果必为合法 UTF-8
    String::from_utf8(out).expect("show 输出必为合法 UTF-8")
}

pub fn quote(v: &str) -> String {
    format!("\"{}\"", show(v))
}

fn report_failure(ctx: &mut Context, label: &str, detail: &str) {
    ctx.failures += 1;
    println!("    [FAIL] {}\n           {}", label, detail);
}

pub fn check_true(ctx: &mut Context, cond: bool, label: &str) {
    ctx.checks += 1;
    if !cond {
        report_failure(ctx, label, "期望为真");
    }
}

#[allow(dead_code)]
pub fn check_false(ctx: &mut Context, cond: bool, label: &str) {
    ctx.checks += 1;
    if cond {
        report_failure(ctx, label, "期望为假");
    }
}

pub fn check_eq_str(ctx: &mut Context, expected: &str, actual: &str, label: &str) {
    ctx.checks += 1;
    if expected != actual {
        report_failure(
            ctx,
            label,
            &format!("期望 {}，实际 {}", quote(expected), quote(actual)),
        );
    }
}

pub fn check_eq_usize(ctx: &mut Context, expected: usize, actual: usize, label: &str) {
    ctx.checks += 1;
    if expected != actual {
        report_failure(ctx, label, &format!("期望 {}，实际 {}", expected, actual));
    }
}

/// 全部用例跑一遍；argv 支持 <名字子串> 与 -v。返回退出码。
pub fn run_all(argv: &[String]) -> i32 {
    let mut filter = String::new();
    for arg in argv {
        if arg == "-v" || arg == "--verbose" {
            // 详细模式占位：本框架已逐用例打印，保留参数只为兼容
        } else if !arg.is_empty() {
            filter = arg.clone();
        }
    }

    let mut case_failed = 0usize;
    let mut case_passed = 0usize;
    let mut total_checks = 0usize;
    let mut total_failures = 0usize;

    println!("==== nevext Rust 单元测试 ====");
    // 用例函数不会反过来注册新用例，持锁遍历是安全的
    let guard = registry().lock().unwrap();
    for (name, f) in guard.iter() {
        if !filter.is_empty() && !name.contains(&filter) {
            continue;
        }
        let mut ctx = Context {
            name: name.clone(),
            checks: 0,
            failures: 0,
        };
        println!("[ RUN  ] {}", name);
        // 用例 panic 要计入失败而不是中断整轮（对应 Python 的 try/except）
        let result = catch_unwind(std::panic::AssertUnwindSafe(|| f(&mut ctx)));
        if result.is_err() {
            ctx.failures += 1;
            println!("    [FAIL] 用例抛出异常: panic");
        }
        total_checks += ctx.checks;
        total_failures += ctx.failures;
        if ctx.failures != 0 {
            case_failed += 1;
            println!(
                "[ FAIL ] {}   失败断言 {}/{}",
                name, ctx.failures, ctx.checks
            );
        } else {
            case_passed += 1;
            println!("[  OK  ] {}   {} 个断言通过", name, ctx.checks);
        }
    }

    println!(
        "--------------------------------------------------\n\
         用例: {}，通过: {}，失败: {}\n\
         断言: {}，失败: {}",
        case_passed + case_failed,
        case_passed,
        case_failed,
        total_checks,
        total_failures
    );
    if case_failed != 0 {
        println!("结果: 失败");
        return 1;
    }
    println!("结果: 全部通过");
    return 0;
}

/// 框架自检：确保 show 的转义与 C++ 侧一致（手工调起，正常测试流程不经过）。
#[allow(dead_code)]
pub fn self_check() -> i32 {
    assert_eq!(show("\n\r\t\x0b\x08\x0c"), "\\n\\r\\t\\v\\b\\f");
    assert_eq!(show("\\"), "\\\\");
    assert_eq!(show("\x01"), "\\x01");
    assert_eq!(show("\x7f"), "\\x7F");
    assert_eq!(show("中"), "中");
    0
}
