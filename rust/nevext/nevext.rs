// ----------------------------------------------------------------------------
// Nevext 的 Rust 实现（Rust 1.70+ / edition 2021，单文件，零依赖）。
//
// 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
// docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
//   · TokenType 五个枚举项的取值顺序与 C++ 相同；
//   · 词法错误进 lexer.errors，语法/转义错误进 parser.errors，消息文本逐字
//     一致（含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
//   · 未闭合的多行注释不是错误，一直延伸到输入结束；
//   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
//   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
//
// Rust 的 String/&str 是 UTF-8 字节序列，本实现按字节扫描（as_bytes +
// 字节下标）。本语法的全部结构字符（空白、; " ' \ /、\n）都在 ASCII 区间，
// 而 UTF-8 编码保证多字节字符不包含 ASCII 字节，因此按字节扫描与按码元
// 扫描行为完全等价；转义表也全部是单字节映射。唯一的实现差异是元素计数
// 按字节（见文档 RS1）。语言限制：&str 必须是合法 UTF-8，孤立代理项与
// 非 UTF-8 字节序列在 Rust 侧无法表达（缺口 RS2）。
//
// 与 C++ 的 API 对应关系（Rust 惯例：枚举变体 PascalCase、错误容器为公开
// 字段、借用代替指针）：
//   Lexer::NextToken()            -> lexer.next_token()
//   Lexer::Reset(input)           -> lexer.reset(code)
//   Parser::ParseProgram(prog, p) -> parser.parse_program(lexer, pause_when_error)
//                                    （语句列表走返回值 ParseResult.val；
//                                     Parser 不绑定词法器，借用检查器友好的
//                                     惯用做法是每次调用显式传入）
//   Parser::Reset(lexer)          -> parser.reset()
// 枚举名用 Status 而非 Result，避免与 std::result::Result 混淆。
//
// 实现说明：与 Python 版同为「完整字符串 + 游标 pos」，行为一致但整体 O(n)。
// ----------------------------------------------------------------------------

// 库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。
pub const VERSION: &str = "0.4.0";

// 错误消息常量，与 C++ 的消息表逐字一致（含句号后的空格）
pub const UNCLOSED_MESSAGE: &str = "unclosed string literal. ";
pub const INVALID_ESCAPE_MESSAGE: &str = "invalid escape character. ";
pub const UNEXPECTED_MESSAGE: &str = "unexpected token. ";

/// 词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（0 起）。
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum TokenType {
    Unexpected = 0, // 非法 token（未闭合引号串、无法推进的字符等）
    EndOfFile = 1,  // 输入结束
    RawStr = 2,     // 未加引号的裸串（不展开转义）
    QuotedStr = 3,  // 引号串（literal 保留转义序列原文，语法层再展开）
    Sep = 4,        // 分号
}

/// 解析结果码。Success 为 0，可直接写 `if res.err == Status::Error`。
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Status {
    Success = 0,
    Error = 1,
}

/// 词法单元。literal 为码元原文；EOF / Unexpected 时为空串。
#[derive(Clone, Debug)]
pub struct Token {
    pub kind: TokenType,
    pub line: usize,
    pub literal: String,
}

impl Default for Token {
    fn default() -> Self {
        Token {
            kind: TokenType::Unexpected,
            line: 0,
            literal: String::new(),
        }
    }
}

/// 一条语句：起始行号 + 解析出的字符串列表。
#[derive(Clone, Debug)]
pub struct Statement {
    pub line: usize,
    pub strings: Vec<String>,
}

/// parse_program 的返回值。err 取 Status；val 为语句列表。
#[derive(Clone, Debug)]
pub struct ParseResult {
    pub err: Status,
    pub val: Vec<Statement>,
}

/// 词法分析器。
pub struct Lexer {
    pub code: String,
    /// 游标：code 中尚未消费部分的起点（字节下标）
    pub pos: usize,
    pub line: usize,
    pub errors: Vec<String>,
}

impl Lexer {
    /// 创建词法分析器。
    pub fn new(code: &str) -> Lexer {
        let mut l = Lexer {
            code: String::new(),
            pos: 0,
            line: 1,
            errors: Vec::new(),
        };
        l.reset(code);
        l
    }

    /// 复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。
    pub fn reset(&mut self, code: &str) {
        self.code.clear();
        self.code.push_str(code);
        self.pos = 0;
        self.line = 1;
        self.errors.clear();
    }

    /// 取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。
    pub fn next_token(&mut self) -> Token {
        let mut token = Token::default();
        self.skip_ws();
        token.line = self.line;

        let bytes = self.code.as_bytes();
        if self.pos >= bytes.len() {
            token.kind = TokenType::EndOfFile;
        } else if bytes[self.pos] == b';' {
            token.kind = TokenType::Sep;
            token.literal = ";".to_string();
            self.pos += 1;
        } else if bytes[self.pos] == b'"' || bytes[self.pos] == b'\'' {
            let quote = bytes[self.pos];
            let (err, val) = self.scan_quoted_string(quote);
            if err != Status::Success {
                // 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
                token.kind = TokenType::Unexpected;
            } else {
                token.kind = TokenType::QuotedStr;
                token.literal = val;
            }
        } else {
            let (err, val) = self.scan_raw_string();
            if err != Status::Success {
                // 一个字符都无法推进（撞上终止字符），吞掉 1 个字节避免死循环
                token.kind = TokenType::Unexpected;
                self.pos += 1;
            } else {
                token.kind = TokenType::RawStr;
                token.literal = val;
            }
        }
        token
    }

    // ------------------------------------------------------------- 私有方法

    /// 引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。
    fn scan_quoted_string(&mut self, quote: u8) -> (Status, String) {
        self.pos += 1; // 跳过开头的引号
        let start = self.pos;
        let mut length = 0usize;
        let bytes = self.code.as_bytes();
        loop {
            if self.pos + length >= bytes.len() {
                self.push_error(UNCLOSED_MESSAGE);
                return (Status::Error, String::new());
            }
            let c = bytes[self.pos + length];
            if c == b'\\' {
                length += 1;
                if self.pos + length >= bytes.len() {
                    self.push_error(UNCLOSED_MESSAGE);
                    return (Status::Error, String::new());
                }
                // 被转义的字符若是换行，物理行号同样要推进
                if bytes[self.pos + length] == b'\n' {
                    self.line += 1;
                }
                length += 1;
            } else if c == quote {
                break;
            } else {
                if c == b'\n' {
                    self.line += 1; // 引号串可以跨行，行号必须跟着走
                }
                length += 1; // 普通字符必须推进，否则死循环
            }
        }
        let val = self.code[start..start + length].to_string();
        self.pos = start + length + 1; // 越过闭合引号
        (Status::Success, val)
    }

    /// 裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。
    fn scan_raw_string(&mut self) -> (Status, String) {
        let start = self.pos;
        let mut length = 0usize;
        let bytes = self.code.as_bytes();
        while self.pos + length < bytes.len() {
            let c = bytes[self.pos + length];
            if is_white(c) || is_raw_str_end(c) {
                break;
            }
            if c == b'/' && self.pos + length + 1 < bytes.len() {
                let nxt = bytes[self.pos + length + 1];
                if nxt == b'/' || nxt == b'*' {
                    break; // 裸串在行注释 / 块注释起始处截断
                }
            }
            length += 1;
        }
        if length == 0 {
            return (Status::Error, String::new());
        }
        let val = self.code[start..start + length].to_string();
        self.pos += length;
        (Status::Success, val)
    }

    /// 跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。
    fn skip_ws(&mut self) {
        loop {
            let bytes = self.code.as_bytes();
            if self.pos >= bytes.len() {
                break;
            }
            let c = bytes[self.pos];
            if is_white(c) {
                if c == b'\n' {
                    self.line += 1;
                }
                self.pos += 1;
            } else if self.test("//") {
                // 单行注释：跳到行尾（含换行符本身）
                match self.code[self.pos..].find('\n') {
                    None => self.pos = self.code.len(),
                    Some(nl) => {
                        self.pos += nl + 1;
                        self.line += 1;
                    }
                }
            } else if self.test("/*") {
                self.skip_multiline_comment();
            } else {
                break;
            }
        }
    }

    fn skip_multiline_comment(&mut self) {
        self.pos += 2; // 跳过 /*
        loop {
            let bytes = self.code.as_bytes();
            if self.pos >= bytes.len() {
                break;
            }
            if bytes[self.pos] == b'*' && self.pos + 1 < bytes.len() && bytes[self.pos + 1] == b'/'
            {
                self.pos += 2;
                break;
            }
            if bytes[self.pos] == b'\n' {
                self.line += 1;
            }
            self.pos += 1;
        }
    }

    /// 判断 pos 处是否以 form 开头。
    fn test(&self, form: &str) -> bool {
        self.code[self.pos..].starts_with(form)
    }

    fn push_error(&mut self, msg: &str) {
        self.errors.push(format!("line {}: {}", self.line, msg));
    }
}

/// 判断是否空白字符（空白与分隔字符集，与 C++ 的 whiteChars 逐项一致）
fn is_white(c: u8) -> bool {
    matches!(c, b' ' | b',' | b'\t' | b'\x0b' | b'\x0c' | b'\n' | b'\r')
}

/// 判断裸串终止字符（与 C++ 的 rawStrEndChars 逐项一致）
fn is_raw_str_end(c: u8) -> bool {
    c == b';' || c == b'"' || c == b'\''
}

/// 转义函数签名：返回 (err, val, reason)；失败时 val 保留已展开前缀，
/// reason 为 "unclosed" | "invalid"。三个内置实现见
/// null_escape / nevext_escape / json_escape。
pub type EscapeFunc = fn(&str) -> (Status, String, &'static str);

/// Nevext 默认转义表（classic 九项 + \uXXXX，v0.4.0 起）。
fn nevext_escape_char(c: u8) -> Option<&'static str> {
    match c {
        b'\\' => Some("\\"),
        b'\'' => Some("'"),
        b'"' => Some("\""),
        b'n' => Some("\n"),
        b'r' => Some("\r"),
        b't' => Some("\t"),
        b'v' => Some("\x0b"),
        b'b' => Some("\x08"),
        b'f' => Some("\x0c"),
        _ => None,
    }
}

/// 严格 JSON 表（表来自 json.org 语法图，与 ECMA-404 一致；不含 \' 与 \v）。
fn json_escape_char(c: u8) -> Option<&'static str> {
    match c {
        b'"' => Some("\""),
        b'\\' => Some("\\"),
        b'/' => Some("/"),
        b'b' => Some("\x08"),
        b'f' => Some("\x0c"),
        b'n' => Some("\n"),
        b'r' => Some("\r"),
        b't' => Some("\t"),
        _ => None,
    }
}

/// 不转义：原样返回（err 恒为 Success）。
pub fn null_escape(src: &str) -> (Status, String, &'static str) {
    (Status::Success, src.to_string(), "")
}

/// Nevext 默认转义（v0.4.0 起）：classic 九项 + \uXXXX。
/// 对 v0.3.0 及之前向后兼容：既有转义序列行为不变，只有 \u 从非法变合法。
/// 失败时 val 保留已展开前缀。
pub fn nevext_escape(src: &str) -> (Status, String, &'static str) {
    escape_with(src, nevext_escape_char, true)
}

/// 严格 JSON 转义：\" \\ \\/ \b \f \n \r \t \uXXXX
/// （表来自 json.org 语法图 / ECMA-404；不含 \' 与 \v）。
/// \uXXXX 按 UTF-8 编码产出；代理区等非法码点 char::from_u32
/// 返回 None，按非法转义处理（见文档差异登记）。
pub fn json_escape(src: &str) -> (Status, String, &'static str) {
    escape_with(src, json_escape_char, true)
}

/// 展开骨架：返回 (err, val, reason)，reason 为 "unclosed" | "invalid"。
fn escape_with(
    src: &str,
    escape_char: fn(u8) -> Option<&'static str>,
    json_unicode: bool,
) -> (Status, String, &'static str) {
    let mut out = String::new();
    let bytes = src.as_bytes();
    let mut i = 0usize;
    while i < bytes.len() {
        let rel = src[i..].find('\\');
        let pos = match rel {
            None => {
                out.push_str(&src[i..]); // 没有转义字符了
                break;
            }
            Some(r) => i + r,
        };
        out.push_str(&src[i..pos]);
        if pos + 1 >= bytes.len() {
            // 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
            return (Status::Error, out, "unclosed");
        }
        let c = bytes[pos + 1];
        if json_unicode && c == b'u' {
            // \uXXXX：4 位十六进制码点（字节级解析，避开 UTF-8 边界 panic）
            if pos + 6 > bytes.len() {
                return (Status::Error, out, "invalid");
            }
            let mut cp: u32 = 0;
            let mut hex_ok = true;
            for k in (pos + 2)..(pos + 6) {
                let b = bytes[k];
                let v = match b {
                    b'0'..=b'9' => (b - b'0') as u32,
                    b'a'..=b'f' => (b - b'a' + 10) as u32,
                    b'A'..=b'F' => (b - b'A' + 10) as u32,
                    _ => {
                        hex_ok = false;
                        break;
                    }
                };
                cp = cp * 16 + v;
            }
            if !hex_ok {
                return (Status::Error, out, "invalid");
            }
            match char::from_u32(cp) {
                Some(ch) => out.push(ch),
                None => return (Status::Error, out, "invalid"),
            }
            i = pos + 6;
            continue;
        }
        match escape_char(c) {
            None => return (Status::Error, out, "invalid"),
            Some(mapped) => out.push_str(mapped),
        }
        i = pos + 2;
    }
    (Status::Success, out, "")
}

/// 展开引号串里的转义序列（历史入口，与 Parser 默认一致；
/// 即 nevext_escape 的同义词）。
pub fn expand_escapes(src: &str) -> (Status, String, &'static str) {
    nevext_escape(src)
}

/// 语法分析器。不绑定词法器（借用检查器友好），每次调用显式传入。
pub struct Parser {
    pub errors: Vec<String>,
    /// 转义函数（默认 nevext_escape：classic 九项 + \uXXXX，v0.4.0 起）。
    /// 可替换为 json_escape / null_escape 或同签名自定义函数。
    pub escape_func: EscapeFunc,
    token: Token,
    failed: bool,
}

impl Default for Parser {
    fn default() -> Parser {
        Parser {
            errors: Vec::new(),
            escape_func: nevext_escape,
            token: Token::default(),
            failed: false,
        }
    }
}

impl Parser {
    /// 创建语法分析器（默认 nevext_escape）。
    pub fn new() -> Parser {
        Parser::default()
    }

    /// 创建语法分析器并指定转义函数。
    pub fn with_escape(escape_func: EscapeFunc) -> Parser {
        let mut p = Parser::new();
        p.escape_func = escape_func;
        p
    }

    /// 清空上一轮的错误记录，复用同一对象（词法器在 parse_program 时传入）。
    /// 转义函数保持不变，需要替换时直接赋值 parser.escape_func。
    pub fn reset(&mut self) {
        self.errors.clear();
        self.token = Token::default();
        self.failed = false;
    }

    /// 解析整个程序。
    /// pause_when_error 为 true（默认）时遇到第一个错误立即停止；
    /// 无论哪种模式，出错前已经解析出的语句都会保留在返回值里。
    pub fn parse_program(&mut self, lexer: &mut Lexer, pause_when_error: bool) -> ParseResult {
        let mut result = ParseResult {
            err: Status::Success,
            val: Vec::new(),
        };
        self.failed = false;
        loop {
            self.token = lexer.next_token();
            match self.token.kind {
                TokenType::EndOfFile => {
                    // 最后一个语句可以没有 ; 结尾，直接退出循环
                }
                TokenType::Sep => {
                    // 空语句，不处理
                }
                TokenType::RawStr | TokenType::QuotedStr => {
                    let mut stmt = Statement {
                        line: self.token.line,
                        strings: Vec::new(),
                    };
                    let lit = self.to_literal(&self.token.clone());
                    stmt.strings.push(lit);
                    self.scan_rest_of_statement(lexer, &mut stmt);
                    result.val.push(stmt);
                }
                TokenType::Unexpected => {
                    // 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
                    // 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
                    self.unexpected_token_error();
                }
            }
            if self.token.kind == TokenType::EndOfFile {
                break;
            }
            if pause_when_error && !self.errors.is_empty() {
                break;
            }
        }
        if self.failed {
            result.err = Status::Error;
        }
        result
    }

    // ------------------------------------------------------------- 私有方法

    /// 语句内部扫描：直到分号 / EOF / 非法 token。
    fn scan_rest_of_statement(&mut self, lexer: &mut Lexer, stmt: &mut Statement) {
        loop {
            self.token = lexer.next_token();
            match self.token.kind {
                TokenType::EndOfFile => return, // 最后一个语句可以没有 ; 结尾
                TokenType::Unexpected => {
                    self.unexpected_token_error();
                    return;
                }
                TokenType::RawStr | TokenType::QuotedStr => {
                    let lit = self.to_literal(&self.token.clone());
                    stmt.strings.push(lit);
                }
                TokenType::Sep => return,
            }
        }
    }

    /// 只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。
    fn to_literal(&mut self, token: &Token) -> String {
        if token.kind != TokenType::QuotedStr {
            return token.literal.clone();
        }
        let (err, val, reason) = (self.escape_func)(&token.literal);
        if err != Status::Success {
            let msg = if reason == "unclosed" {
                UNCLOSED_MESSAGE
            } else {
                INVALID_ESCAPE_MESSAGE
            };
            self.errors.push(format!("line {}: {}", token.line, msg));
            self.failed = true;
        }
        val
    }

    fn unexpected_token_error(&mut self) {
        self.errors
            .push(format!("line {}: {}", self.token.line, UNEXPECTED_MESSAGE));
        self.failed = true;
    }
    // 说明：C++ / Python 侧的 undefined token type 兜底分支在 Rust 里不需要——
    // match 对 TokenType 的五个取值要求穷尽，语法上不存在未处理的类型。
}
