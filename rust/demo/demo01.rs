// ----------------------------------------------------------------------------
// demo01.rs —— Nevext Rust 实现的示例程序。
//
// 运行：
//   cargo run --bin demo01              解析内置示例
//   cargo run --bin demo01 -- 某个文件   解析该文件（UTF-8）
//
// 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此所有实现
// （C++ / GDScript / JavaScript / C# / Python / Lua / Go / Rust）的 demo
// 输出可以互相 diff，见 docs/zh-cn/test-plan.md 的「demo 门禁」。
//
// 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
// ----------------------------------------------------------------------------

use std::process::exit;
use std::{env, fs};

use nevext::{Lexer, Parser};

/// 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
const BUILTIN_PARTS: [&str; 6] = [
    "  // 单行注释",
    "  hello \"world\";",
    "  /*            ",
    "  多行注释      ",
    "  */            ",
    "  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾",
];

fn builtin() -> String {
    BUILTIN_PARTS.join("\n")
}

/// 无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 None。
fn read_input() -> Option<String> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        return Some(builtin());
    }
    let path = &args[1];
    match fs::read(path) {
        Ok(bytes) => {
            // utf-8-sig 顺带吃掉可能存在的 UTF-8 BOM，与其他实现口径一致
            let text = String::from_utf8_lossy(&bytes);
            Some(text.strip_prefix('\u{feff}').unwrap_or(&text).to_string())
        }
        Err(_) => {
            // 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
            eprintln!("cannot open: {}", path);
            None
        }
    }
}

fn main() {
    let src = match read_input() {
        None => exit(2),
        Some(s) => s,
    };

    let mut lexer = Lexer::new(&src);
    let mut parser = Parser::new();
    let res = parser.parse_program(&mut lexer, true);

    if res.err == nevext::Status::Error {
        // 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
        println!("==== Lexer errors =============================");
        for e in &lexer.errors {
            println!("{}", e);
        }
        println!("==== Parser errors =============================");
        for e in &parser.errors {
            println!("{}", e);
        }
        exit(1);
    }

    println!("statements num: {}", res.val.len());
    for (i, stmt) in res.val.iter().enumerate() {
        println!("statement {} at line {}:", i + 1, stmt.line);
        // 与 C++ 一致：每个串后跟一个空格
        let mut line = String::from("  ");
        for s in &stmt.strings {
            line.push_str(s);
            line.push(' ');
        }
        println!("{}", line);
    }
    exit(0);
}
