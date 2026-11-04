// ----------------------------------------------------------------------------
// test_impl.rs —— Rust 实现私有的用例（与 Python 侧 test_impl.py 对应）
//
// 通用语料（test_conformance.rs）各实现共用；本文件放 Rust 自己关心的
// 工程性用例：对象复用、终止性、长裸串、大输入，以及一条字节模型用例。
// ----------------------------------------------------------------------------

use crate::test_framework::{check_eq_str, check_eq_usize, register, Context};
use nevext::{Lexer, Parser, Status, TokenType};

// RS-01 Parser.reset 能让实例复用到另一份源码
//       （与 C++ 的 LEX-26 / PAR-18、Python 的 PY-01 对应。
//        Rust 侧 Parser 不绑定词法器，reset 只清错误容器；复用体现在
//        同一个 Parser 实例被再次用于新输入）
fn reset_is_reusable(ctx: &mut Context) {
    let mut lexer = Lexer::new("a b");
    let mut parser = Parser::new();
    let first = parser.parse_program(&mut lexer, true);
    check_eq_usize(ctx, 1, first.val.len(), "第一次语句数");
    check_eq_str(ctx, "Success", status_name(first.err), "第一次返回值");

    lexer.reset("c; d");
    parser.reset();
    let second = parser.parse_program(&mut lexer, true);
    check_eq_usize(ctx, 2, second.val.len(), "第二次语句数");
    if second.val.len() >= 2 {
        check_eq_str(ctx, "c", &second.val[0].strings[0], "第 1 条语句");
        check_eq_str(ctx, "d", &second.val[1].strings[0], "第 2 条语句");
    }
    check_eq_usize(ctx, 0, parser.errors.len(), "Parser 错误数（reset 应清空）");
    check_eq_usize(ctx, 0, lexer.errors.len(), "Lexer 错误数（reset 应清空）");
    check_eq_usize(ctx, 1, lexer.line, "行号应回到 1");
}

// RS-02 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
fn pathological_inputs_terminate(ctx: &mut Context) {
    let src = "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;";
    for i in 0..=src.len() {
        let mut lexer = Lexer::new(&src[..i]);
        let mut reached_eof = false;
        for _ in 0..200 {
            if lexer.next_token().kind == TokenType::EndOfFile {
                reached_eof = true;
                break;
            }
        }
        check_true_any(ctx, reached_eof, &format!("前缀[{}]能在阈值内扫到 EOF", i));
    }
}

// RS-03 长裸串不被截断
fn long_raw_token_is_complete(ctx: &mut Context) {
    let src = format!("{};", "x".repeat(1000));
    let mut lexer = Lexer::new(&src);
    let token = lexer.next_token();
    check_eq_str(ctx, "RawStr", token_type_name(token.kind), "token 类型");
    check_eq_usize(ctx, 1000, token.literal.len(), "裸串长度");
}

// RS-04 大量语句全部解析（游标实现不应有性能悬崖）
fn many_statements_are_all_parsed(ctx: &mut Context) {
    let mut src = String::new();
    for i in 0..500 {
        src.push_str(&format!("s{} a b;\n", i));
    }
    let mut lexer = Lexer::new(&src);
    let mut parser = Parser::new();
    let res = parser.parse_program(&mut lexer, true);
    check_eq_usize(ctx, 500, res.val.len(), "语句数");
    check_eq_str(ctx, "Success", status_name(res.err), "返回值");
    check_eq_str(ctx, "s0", &res.val[0].strings[0], "首语句首串");
    check_eq_str(ctx, "s499", &res.val[499].strings[0], "末语句首串");
    check_eq_usize(ctx, 500, res.val[499].line, "末语句行号");
}

// RS-05 多字节字符按 UTF-8 原样保留（Rust 的 &str 是 UTF-8 字节序列；
// len() 计的是字节数，元素计数口径见缺口 RS1。孤立代理项 / 非 UTF-8
// 字节在 &str 里无法表达，属语言限制，见缺口 RS2）
fn multibyte_chars_preserved(ctx: &mut Context) {
    let mut lexer = Lexer::new("x \u{00e9}\u{4e2d}\u{1f600} y");
    let mut parser = Parser::new();
    let res = parser.parse_program(&mut lexer, true);
    check_eq_usize(ctx, 1, res.val.len(), "语句数");
    if res.val.len() >= 1 {
        let strs = &res.val[0].strings;
        check_eq_usize(ctx, 3, strs.len(), "字符串数");
        if strs.len() >= 3 {
            check_eq_str(
                ctx,
                "\u{00e9}\u{4e2d}\u{1f600}",
                &strs[1],
                "多字节字符原样保留",
            );
            check_eq_usize(ctx, 9, strs[1].len(), "元素计数按字节（缺口 RS1）");
        }
    }
    check_eq_usize(ctx, 0, lexer.errors.len() + parser.errors.len(), "错误数");
}

fn check_true_any(ctx: &mut Context, cond: bool, label: &str) {
    crate::test_framework::check_true(ctx, cond, label);
}

fn status_name(s: Status) -> &'static str {
    if s == Status::Success {
        "Success"
    } else {
        "Error"
    }
}

fn token_type_name(t: TokenType) -> &'static str {
    if t == TokenType::RawStr {
        "RawStr"
    } else {
        "其它"
    }
}

pub fn register_all() {
    register("Impl.ResetIsReusable", reset_is_reusable);
    register(
        "Impl.PathologicalInputsTerminate",
        pathological_inputs_terminate,
    );
    register("Impl.LongRawTokenIsComplete", long_raw_token_is_complete);
    register(
        "Impl.ManyStatementsAreAllParsed",
        many_statements_are_all_parsed,
    );
    register("Impl.MultibyteCharsPreserved", multibyte_chars_preserved);
}
