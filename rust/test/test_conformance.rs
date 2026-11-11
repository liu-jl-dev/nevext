// ----------------------------------------------------------------------------
// test_conformance.rs —— 跨实现通用语料测试（55 条，A01 ~ H04）
//
// 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
// cpp/test/test_conformance.cpp，Python 侧见 python/test/test_conformance.py，
// 其余实现同构。各实现必须完全一致：ID、输入、pause 开关、期望的返回值/
// 产物/错误序列。本文件只做「驱动」，不含任何期望值推导。
//
// 输入与期望串一律用 Rust 转义书写：\n \r \t \v \b \f \\ \" \u{XXXX}，
// 与文档「避免源文件编码差异」的约定一致。
// 注意 H02：Rust 的 &str 是 UTF-8 字节序列，emoji 写成 \u{1F600}（运行期
// 落成 4 字节 UTF-8）；C++ / JS / C# 内部是 UTF-16 码元。两者是同一字符，
// 通用语料只断言内容相等（见文档 X2）。
// ----------------------------------------------------------------------------

use std::sync::OnceLock;

use crate::test_framework::{check_eq_str, check_eq_usize, register, show, Context};
use nevext::{Lexer, Parser, Statement, Status};

// ------------------------------------------------------------------ 期望值表

pub struct StmtSpec {
    pub line: usize,
    pub strings: Vec<&'static str>,
}

pub struct ErrSpec {
    pub line: usize,
    pub kind: &'static str,
}

pub struct CaseSpec {
    pub id: &'static str,
    pub src: &'static str,
    pub ok: bool,
    pub pause: bool,
    pub prog: Vec<StmtSpec>,
    pub errs: Vec<ErrSpec>,
}

fn st(line: usize, strings: &[&'static str]) -> StmtSpec {
    StmtSpec {
        line,
        strings: strings.to_vec(),
    }
}

fn er(line: usize, kind: &'static str) -> ErrSpec {
    ErrSpec { line, kind }
}

fn cases() -> &'static Vec<CaseSpec> {
    static CASES: OnceLock<Vec<CaseSpec>> = OnceLock::new();
    CASES.get_or_init(build_cases)
}

fn build_cases() -> Vec<CaseSpec> {
    vec![
        // ---- A 组 程序骨架与语句切分
        CaseSpec {
            id: "A01",
            src: "",
            ok: true,
            pause: true,
            prog: vec![],
            errs: vec![],
        },
        CaseSpec {
            id: "A02",
            src: "abc",
            ok: true,
            pause: true,
            prog: vec![st(1, &["abc"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A03",
            src: "a, b\tc",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b", "c"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A04",
            src: "a b; c d; e",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b"]), st(1, &["c", "d"]), st(1, &["e"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A05",
            src: "a; b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"]), st(1, &["b"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A06",
            src: ";;;a;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A07",
            src: "a,,b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A08",
            src: "  \t\n ",
            ok: true,
            pause: true,
            prog: vec![],
            errs: vec![],
        },
        CaseSpec {
            id: "A09",
            src: "a b\nc d;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b", "c", "d"])],
            errs: vec![],
        },
        CaseSpec {
            id: "A10",
            src: "windowSize 1280, 720;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["windowSize", "1280", "720"])],
            errs: vec![],
        },
        // ---- B 组 引号串
        CaseSpec {
            id: "B01",
            src: "\"hello world\" x",
            ok: true,
            pause: true,
            prog: vec![st(1, &["hello world", "x"])],
            errs: vec![],
        },
        CaseSpec {
            id: "B02",
            src: "'abc'",
            ok: true,
            pause: true,
            prog: vec![st(1, &["abc"])],
            errs: vec![],
        },
        CaseSpec {
            id: "B03",
            src: "\"\" z",
            ok: true,
            pause: true,
            prog: vec![st(1, &["", "z"])],
            errs: vec![],
        },
        CaseSpec {
            id: "B04",
            src: "'a' \"b\" c",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b", "c"])],
            errs: vec![],
        },
        CaseSpec {
            id: "B05",
            src: "'\"' \"'\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["\"", "'"])],
            errs: vec![],
        },
        CaseSpec {
            id: "B06",
            src: "\"a\"b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b"])],
            errs: vec![],
        },
        // ---- C 组 转义（只在语法层对 QuotedStr 展开；v0.4.0 起默认 nevext_escape：
        // classic 九项 + \uXXXX，对 v0.3.0 及之前向后兼容）
        CaseSpec {
            id: "C01",
            src: "\"a\\nb\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\nb"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C02",
            src: "\"\\n\\r\\t\\v\\b\\f\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["\n\r\t\x0b\x08\x0c"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C03",
            src: "\"a\\\\\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\\"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C04",
            src: "\"ab\\\"cd\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["ab\"cd"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C05",
            src: "'a\\qb'",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        CaseSpec {
            id: "C06",
            src: "\"a\\qb\"; c",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        CaseSpec {
            id: "C07",
            src: "\"a\\qb\"; c",
            ok: false,
            pause: false,
            prog: vec![st(1, &["a"]), st(1, &["c"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        // X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
        CaseSpec {
            id: "C08",
            src: "\"a\\\nb\"",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        CaseSpec {
            id: "C09",
            src: "\"x\\ny\\tz\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["x\ny\tz"])],
            errs: vec![],
        },
        // ---- C 组 \u 扩展（v0.4.0；\u 表项与 json.org 语法图 / ECMA-404 一致）
        // \/ 不在默认表（json_escape 才有）：报错并保留前缀
        CaseSpec {
            id: "C10",
            src: "\"a\\/b\"",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        CaseSpec {
            id: "C11",
            src: "\"\\u4e2d\\u6587\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["\u{4e2d}\u{6587}"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C12",
            src: "\"\\u4E2D\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["\u{4e2d}"])],
            errs: vec![],
        },
        CaseSpec {
            id: "C13",
            src: "\"a\\ub\"",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        // \' 在默认表合法（Nevext 两种引号串都需要它；json_escape 下才非法）
        CaseSpec {
            id: "C14",
            src: "'a\\'b'",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a'b"])],
            errs: vec![],
        },
        // ---- D 组 裸串边界（裸串不展开转义）
        CaseSpec {
            id: "D01",
            src: "a\\b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\\b"])],
            errs: vec![],
        },
        CaseSpec {
            id: "D02",
            src: "a\\nb",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\\nb"])],
            errs: vec![],
        },
        CaseSpec {
            id: "D03",
            src: "a\\qb",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\\qb"])],
            errs: vec![],
        },
        CaseSpec {
            id: "D04",
            src: "a\"b",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        CaseSpec {
            id: "D05",
            src: "a\"b",
            ok: false,
            pause: false,
            prog: vec![st(1, &["a"]), st(1, &["b"])],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        CaseSpec {
            id: "D06",
            src: "a'b",
            ok: false,
            pause: false,
            prog: vec![st(1, &["a"]), st(1, &["b"])],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        CaseSpec {
            id: "D07",
            src: "a//comment",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![],
        },
        CaseSpec {
            id: "D08",
            src: "a/**/ b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b"])],
            errs: vec![],
        },
        CaseSpec {
            id: "D09",
            src: "\"a//b\"",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a//b"])],
            errs: vec![],
        },
        // 退格不是裸串终止字符：整串是一个 RawStr（Rust 字面量无 \b 转义，写 \x08）
        CaseSpec {
            id: "D10",
            src: "a\x08b",
            ok: true,
            pause: false,
            prog: vec![st(1, &["a\x08b"])],
            errs: vec![],
        },
        // ---- E 组 注释（未闭合的多行注释不是错误）
        CaseSpec {
            id: "E01",
            src: "// only comment",
            ok: true,
            pause: true,
            prog: vec![],
            errs: vec![],
        },
        CaseSpec {
            id: "E02",
            src: "/* c1\nc2 */ d",
            ok: true,
            pause: true,
            prog: vec![st(2, &["d"])],
            errs: vec![],
        },
        CaseSpec {
            id: "E03",
            src: "a /* xx",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![],
        },
        CaseSpec {
            id: "E04",
            src: "a /* x\nb; c;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![],
        },
        CaseSpec {
            id: "E05",
            src: "/**/a",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![],
        },
        CaseSpec {
            id: "E06",
            src: "a //c1\n/* c2 */ b",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a", "b"])],
            errs: vec![],
        },
        // ---- F 组 行号
        CaseSpec {
            id: "F01",
            src: "a;\nb;\nc;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"]), st(2, &["b"]), st(3, &["c"])],
            errs: vec![],
        },
        CaseSpec {
            id: "F02",
            src: "a;\r\nb;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"]), st(2, &["b"])],
            errs: vec![],
        },
        CaseSpec {
            id: "F03",
            src: "\"a\nb\"; c;",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a\nb"]), st(2, &["c"])],
            errs: vec![],
        },
        CaseSpec {
            id: "F04",
            src: "\"a\\\nb\"; c;",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "InvalidEscape")],
        },
        // 词法错误报在串结束行，语法错误报在串起始行
        CaseSpec {
            id: "F05",
            src: "a \"b\nc",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(2, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        // ---- G 组 错误与错误恢复
        CaseSpec {
            id: "G01",
            src: "a \"bc",
            ok: false,
            pause: true,
            prog: vec![st(1, &["a"])],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        CaseSpec {
            id: "G02",
            src: "a \"bc",
            ok: false,
            pause: false,
            prog: vec![st(1, &["a"]), st(1, &["bc"])],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        // 同一行的多处错误都要记录
        CaseSpec {
            id: "G03",
            src: "\"a\\qb\" \"c\\qd\"; e",
            ok: false,
            pause: false,
            prog: vec![st(1, &["a", "c"]), st(1, &["e"])],
            errs: vec![er(1, "InvalidEscape"), er(1, "InvalidEscape")],
        },
        CaseSpec {
            id: "G04",
            src: "\"abc",
            ok: false,
            pause: true,
            prog: vec![],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        CaseSpec {
            id: "G05",
            src: "\"a\nb",
            ok: false,
            pause: true,
            prog: vec![],
            errs: vec![er(2, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        // ---- H 组 编码与其它
        CaseSpec {
            id: "H01",
            src: "a;\"b\";c",
            ok: true,
            pause: true,
            prog: vec![st(1, &["a"]), st(1, &["b"]), st(1, &["c"])],
            errs: vec![],
        },
        // 非 BMP 字符：Rust 用 \u{1F600} 通用转义写成 4 字节 UTF-8；C++ / JS / C# 内部是代理对
        CaseSpec {
            id: "H02",
            src: "\"\u{1F600}\" x",
            ok: true,
            pause: true,
            prog: vec![st(1, &["\u{1F600}", "x"])],
            errs: vec![],
        },
        CaseSpec {
            id: "H03",
            src: "\"",
            ok: false,
            pause: true,
            prog: vec![],
            errs: vec![er(1, "UnclosedString"), er(1, "UnexpectedToken")],
        },
        // 同样为了避免源文件编码差异，非 ASCII 一律写 \u{XXXX}
        CaseSpec {
            id: "H04",
            src:
                "\u{6d77}\u{4e0a}\u{751f}\u{660e}\u{6708} \u{5929}\u{6daf}\u{5171}\u{6b64}\u{65f6}",
            ok: true,
            pause: true,
            prog: vec![st(
                1,
                &[
                    "\u{6d77}\u{4e0a}\u{751f}\u{660e}\u{6708}",
                    "\u{5929}\u{6daf}\u{5171}\u{6b64}\u{65f6}",
                ],
            )],
            errs: vec![],
        },
    ]
}

// ------------------------------------------------------------------ 驱动逻辑

pub struct RunOut {
    pub result: Status,
    pub prog: Vec<Statement>,
    pub lex_errors: Vec<String>,
    pub parse_errors: Vec<String>,
}

/// 一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。
pub fn run(src: &str, pause_when_error: bool) -> RunOut {
    let mut lexer = Lexer::new(src);
    let mut parser = Parser::new();
    let res = parser.parse_program(&mut lexer, pause_when_error);
    RunOut {
        result: res.err,
        prog: res.val,
        lex_errors: lexer.errors,
        parse_errors: parser.errors,
    }
}

/// 错误消息 -> (行号, 种类) 语义序列。词法错误在前，语法错误在后。
pub fn sem_errors(out: &RunOut) -> Vec<String> {
    fn kind_of(msg: &str) -> String {
        if msg.contains("unclosed string literal") {
            "UnclosedString".to_string()
        } else if msg.contains("invalid escape character") {
            "InvalidEscape".to_string()
        } else if msg.contains("unexpected token") {
            "UnexpectedToken".to_string()
        } else {
            format!("RAW({})", msg)
        }
    }
    fn line_of(msg: &str) -> i64 {
        let pos = match msg.find("line ") {
            None => return -1,
            Some(p) => p,
        };
        let mut digits = String::new();
        for ch in msg[pos + 5..].chars() {
            if !ch.is_ascii_digit() {
                break;
            }
            digits.push(ch);
        }
        if digits.is_empty() {
            return -1;
        }
        digits.parse().unwrap_or(-1)
    }
    let mut res = Vec::new();
    for e in out.lex_errors.iter().chain(out.parse_errors.iter()) {
        res.push(format!("{}:{}", line_of(e), kind_of(e)));
    }
    res
}

fn find_case(id: &str) -> Option<&'static CaseSpec> {
    cases().iter().find(|c| c.id == id)
}

fn check_case(ctx: &mut Context, case_id: &str) {
    let want = match find_case(case_id) {
        None => {
            ctx.failures += 1;
            println!("    [FAIL] 语料里没有这条用例: {}", case_id);
            return;
        }
        Some(w) => w,
    };

    let out = run(want.src, want.pause);

    // 1) 返回值
    let want_result = if want.ok { "Success" } else { "Error" };
    let got_result = if out.result == Status::Success {
        "Success"
    } else {
        "Error"
    };
    check_eq_str(ctx, want_result, got_result, "返回值");

    // 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
    check_eq_usize(ctx, want.prog.len(), out.prog.len(), "语句数");
    for i in 0..out.prog.len().min(want.prog.len()) {
        check_eq_usize(
            ctx,
            want.prog[i].line,
            out.prog[i].line,
            &format!("语句[{}]行号", i),
        );
        check_eq_usize(
            ctx,
            want.prog[i].strings.len(),
            out.prog[i].strings.len(),
            &format!("语句[{}]字符串数", i),
        );
        for j in 0..out.prog[i].strings.len().min(want.prog[i].strings.len()) {
            check_eq_str(
                ctx,
                want.prog[i].strings[j],
                &out.prog[i].strings[j],
                &format!("语句[{}]字符串[{}]", i, j),
            );
        }
    }

    // 3) 错误：按 (行号,种类) 序列比较，顺序敏感
    let got = sem_errors(&out);
    let exp: Vec<String> = want
        .errs
        .iter()
        .map(|e| format!("{}:{}", e.line, e.kind))
        .collect();
    check_eq_usize(ctx, exp.len(), got.len(), "错误数");
    for i in 0..got.len().min(exp.len()) {
        check_eq_str(ctx, &exp[i], &got[i], &format!("错误[{}]", i));
    }
}

/// dump 用：show 已把 \ 转成 \\；在其之后再转义引号才是安全的。
fn quote_dump(s: &str) -> String {
    s.replace('"', "\\\"")
}

/// 按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。
pub fn dump_conformance() {
    for c in cases() {
        let out = run(c.src, c.pause);
        let parts: Vec<String> = out
            .prog
            .iter()
            .map(|stmt| {
                let quoted: Vec<String> = stmt
                    .strings
                    .iter()
                    .map(|s| format!("\"{}\"", quote_dump(&show(s))))
                    .collect();
                format!("{}:[{}]", stmt.line, quoted.join(","))
            })
            .collect();
        let prog = parts.join("|");
        let errs = sem_errors(&out).join(",");
        let ok = if out.result == Status::Success {
            "OK"
        } else {
            "Err"
        };
        println!(
            "{}|{}|{}|{}|pause={}",
            c.id,
            ok,
            prog,
            errs,
            if c.pause { "1" } else { "0" }
        );
    }
}

// ------------------------------------------------------------------ 用例注册
// 每条语料一个独立用例名，支持 cargo run --bin nevext_test -- Conformance.C0 前缀筛选。

pub fn register_all() {
    for c in cases() {
        let id = c.id;
        // 无捕获闭包可强转成函数指针，满足框架的 CaseFn 签名
        register(&format!("Conformance.{}", id), move |ctx| {
            check_case(ctx, id)
        });
    }
}
