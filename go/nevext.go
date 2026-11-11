// ----------------------------------------------------------------------------
// Nevext 的 Go 实现（Go 1.21+，单文件，零依赖）。
//
// 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
// docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
//   · TokenType 五个枚举项的取值顺序与 C++ 相同；
//   · 词法错误进 lexer.Errors，语法/转义错误进 parser.Errors，消息文本逐字
//     一致（含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
//   · 未闭合的多行注释不是错误，一直延伸到输入结束；
//   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
//   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
//
// Go 的 string 是 UTF-8 字节序列，按字节扫描。本语法的全部结构字符
// （空白、; " ' \ /、\n）都在 ASCII 区间，而 UTF-8 编码保证多字节字符
// 不包含 ASCII 字节，因此按字节扫描与按码元扫描行为完全等价；转义表
// 也全部是单字节映射。唯一的实现差异是元素计数按字节（见文档 GO1）。
//
// 与 C++ 的 API 对应关系（Go 惯例 PascalCase 导出、错误容器为公开字段）：
//   Lexer::NextToken()            -> lexer.NextToken()
//   Lexer::Reset(input)           -> lexer.Reset(code)
//   Parser::ParseProgram(prog, p) -> parser.ParseProgram(pauseWhenError)
//                                    （语句列表走返回值 ParseResult.Val）
//   Parser::Reset(lexer)          -> parser.Reset(lexer)
//
// 实现说明：与 Python 版同为「完整字符串 + 游标 Pos」，行为一致但整体 O(n)。
// ----------------------------------------------------------------------------

package nevext

import (
	"strconv"
	"strings"
)

// 库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。
const Version = "0.4.0"

// 错误消息常量，与 C++ 的消息表逐字一致（含句号后的空格）
const (
	UnclosedMessage      = "unclosed string literal. "
	InvalidEscapeMessage = "invalid escape character. "
	UnexpectedMessage    = "unexpected token. "
)

// isWhite 判断是否空白字符（空白与分隔字符集，与 C++ 的 whiteChars 逐项一致）
func isWhite(c byte) bool {
	switch c {
	case ' ', ',', '\t', '\v', '\f', '\n', '\r':
		return true
	}
	return false
}

// isRawStrEnd 判断裸串终止字符（与 C++ 的 rawStrEndChars 逐项一致）
func isRawStrEnd(c byte) bool {
	return c == ';' || c == '"' || c == '\''
}

// TokenType 词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（0 起）。
type TokenType int

const (
	Unexpected TokenType = iota // 非法 token（未闭合引号串、无法推进的字符等）
	EndOfFile                   // 输入结束
	RawStr                      // 未加引号的裸串（不展开转义）
	QuotedStr                   // 引号串（Literal 保留转义序列原文，语法层再展开）
	Sep                         // 分号
)

// Result 解析结果码。Success 为 0。
type Result int

const (
	Success Result = 0
	Error   Result = 1
)

// Token 词法单元。Literal 为码元原文；EOF / Unexpected 时为空串。
type Token struct {
	Type    TokenType
	Line    int
	Literal string
}

// Statement 一条语句：起始行号 + 解析出的字符串列表。
type Statement struct {
	Line    int
	Strings []string
}

// ParseResult ParseProgram 的返回值。Err 取 Result；Val 为语句列表。
type ParseResult struct {
	Err Result
	Val []Statement
}

// Lexer 词法分析器。
type Lexer struct {
	Code   string
	Pos    int // 游标：Code 中尚未消费部分的起点（字节下标）
	Line   int
	Errors []string
}

// NewLexer 创建词法分析器。
func NewLexer(code string) *Lexer {
	l := &Lexer{}
	l.Reset(code)
	return l
}

// Reset 复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。
func (l *Lexer) Reset(code string) {
	l.Code = code
	l.Pos = 0
	l.Line = 1
	l.Errors = nil
}

// NextToken 取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。
func (l *Lexer) NextToken() Token {
	token := Token{Type: Unexpected}
	l.skipWS()
	token.Line = l.Line

	if l.Pos >= len(l.Code) {
		token.Type = EndOfFile
	} else if l.Code[l.Pos] == ';' {
		token.Type = Sep
		token.Literal = ";"
		l.Pos++
	} else if l.Code[l.Pos] == '"' || l.Code[l.Pos] == '\'' {
		err, val := l.scanQuotedString(l.Code[l.Pos])
		if err != Success {
			// 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
			token.Type = Unexpected
		} else {
			token.Type = QuotedStr
			token.Literal = val
		}
	} else {
		err, val := l.scanRawString()
		if err != Success {
			// 一个字符都无法推进（撞上终止字符），吞掉 1 个字节避免死循环
			token.Type = Unexpected
			l.Pos++
		} else {
			token.Type = RawStr
			token.Literal = val
		}
	}
	return token
}

// ------------------------------------------------------------- 私有方法

// scanQuotedString 引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。
func (l *Lexer) scanQuotedString(quote byte) (Result, string) {
	l.Pos++ // 跳过开头的引号
	start := l.Pos
	length := 0
	for {
		if l.Pos+length >= len(l.Code) {
			l.pushError(UnclosedMessage)
			return Error, ""
		}
		c := l.Code[l.Pos+length]
		if c == '\\' {
			length++
			if l.Pos+length >= len(l.Code) {
				l.pushError(UnclosedMessage)
				return Error, ""
			}
			// 被转义的字符若是换行，物理行号同样要推进
			if l.Code[l.Pos+length] == '\n' {
				l.Line++
			}
			length++
		} else if c == quote {
			break
		} else {
			if c == '\n' {
				l.Line++ // 引号串可以跨行，行号必须跟着走
			}
			length++ // 普通字符必须推进，否则死循环
		}
	}
	val := l.Code[start : start+length]
	l.Pos = start + length + 1 // 越过闭合引号
	return Success, val
}

// scanRawString 裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。
func (l *Lexer) scanRawString() (Result, string) {
	start := l.Pos
	length := 0
	for l.Pos+length < len(l.Code) {
		c := l.Code[l.Pos+length]
		if isWhite(c) || isRawStrEnd(c) {
			break
		}
		if c == '/' && l.Pos+length+1 < len(l.Code) {
			nxt := l.Code[l.Pos+length+1]
			if nxt == '/' || nxt == '*' {
				break // 裸串在行注释 / 块注释起始处截断
			}
		}
		length++
	}
	if length <= 0 {
		return Error, ""
	}
	l.Pos += length
	return Success, l.Code[start : start+length]
}

// skipWS 跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。
func (l *Lexer) skipWS() {
	for l.Pos < len(l.Code) {
		c := l.Code[l.Pos]
		if isWhite(c) {
			if c == '\n' {
				l.Line++
			}
			l.Pos++
		} else if l.test("//") {
			// 单行注释：跳到行尾（含换行符本身）
			nl := strings.IndexByte(l.Code[l.Pos:], '\n')
			if nl < 0 {
				l.Pos = len(l.Code)
			} else {
				l.Pos += nl + 1
				l.Line++
			}
		} else if l.test("/*") {
			l.skipMultilineComment()
		} else {
			break
		}
	}
}

func (l *Lexer) skipMultilineComment() {
	l.Pos += 2 // 跳过 /*
	for l.Pos < len(l.Code) {
		if l.test("*/") {
			l.Pos += 2
			break
		}
		if l.Code[l.Pos] == '\n' {
			l.Line++
		}
		l.Pos++
	}
}

// test 判断 Pos 处是否以 form 开头。
func (l *Lexer) test(form string) bool {
	return strings.HasPrefix(l.Code[l.Pos:], form)
}

func (l *Lexer) pushError(msg string) {
	l.Errors = append(l.Errors, "line "+strconv.Itoa(l.Line)+": "+msg)
}

// 转义表两套：Nevext 默认表（classic 九项 + \uXXXX，v0.4.0 起）与严格 JSON 表。
// JSON 转义表来自 json.org 语法图（与 ECMA-404 一致）：
// \" \\\\ \\/ \b \f \n \r \t \uXXXX（4 位十六进制码点，大小写均可；
// 不含 \' 与 \v）。三种转义函数见 NullEscape / NevextEscape / JsonEscape；
// v0.4.0 起 Parser 默认使用 NevextEscape（对 v0.3.0 及之前向后兼容）。
var nevextEscapeMap = map[byte]string{
	'\\': "\\",
	'\'': "'",
	'"':  "\"",
	'n':  "\n",
	'r':  "\r",
	't':  "\t",
	'v':  "\v",
	'b':  "\b",
	'f':  "\f",
}

var jsonEscapeMap = map[byte]string{
	'"':  "\"",
	'\\': "\\",
	'/':  "/",
	'b':  "\b",
	'f':  "\f",
	'n':  "\n",
	'r':  "\r",
	't':  "\t",
}

// EscapeFunc 转义函数签名：返回 (err, val, reason)；
// 失败时 val 保留已展开前缀，reason 为 "unclosed" | "invalid"。
// 三个内置实现见 NullEscape / NevextEscape / JsonEscape。
type EscapeFunc func(src string) (Result, string, string)

// NullEscape 不转义：原样返回（err 恒为 Success）。
func NullEscape(src string) (Result, string, string) {
	return Success, src, ""
}

// NevextEscape Nevext 默认转义（v0.4.0 起）：classic 九项 + \uXXXX。
// 对 v0.3.0 及之前向后兼容：既有转义序列行为不变，只有 \u 从非法变合法。
func NevextEscape(src string) (Result, string, string) {
	return escapeWith(src, nevextEscapeMap, true)
}

// JsonEscape 严格 JSON 转义：\" \\\\ \\/ \b \f \n \r \t \uXXXX
// （表来自 json.org 语法图 / ECMA-404；不含 \' 与 \v）。
// \uXXXX 按 UTF-8 编码产出；代理区码点 string(rune) 会落成 U+FFFD
// （Go 的 rune→string 规则，见文档差异登记）。
func JsonEscape(src string) (Result, string, string) {
	return escapeWith(src, jsonEscapeMap, true)
}

// escapeWith 统一展开骨架。
func escapeWith(src string, m map[byte]string, jsonUnicode bool) (Result, string, string) {
	var out []byte
	i := 0
	for i < len(src) {
		rel := strings.IndexByte(src[i:], '\\')
		if rel < 0 {
			out = append(out, src[i:]...) // 没有转义字符了
			break
		}
		pos := i + rel
		out = append(out, src[i:pos]...)
		if pos+1 >= len(src) {
			// 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
			return Error, string(out), "unclosed"
		}
		c := src[pos+1]
		if jsonUnicode && c == 'u' {
			if pos+6 > len(src) {
				return Error, string(out), "invalid"
			}
			cp, err := strconv.ParseUint(src[pos+2:pos+6], 16, 32)
			if err != nil {
				return Error, string(out), "invalid"
			}
			out = append(out, string(rune(cp))...)
			i = pos + 6
			continue
		}
		mapped, ok := m[c]
		if !ok {
			return Error, string(out), "invalid"
		}
		out = append(out, mapped...)
		i = pos + 2
	}
	return Success, string(out), ""
}

// ExpandEscapes 展开引号串里的转义序列（历史入口，与 Parser 默认一致；
// 即 NevextEscape 的同义词）。
func ExpandEscapes(src string) (Result, string, string) {
	return NevextEscape(src)
}

// Parser 语法分析器。
type Parser struct {
	Lexer      *Lexer
	Errors     []string
	EscapeFunc EscapeFunc // 转义函数（默认 NevextEscape，可替换）
	token      Token
	failed     bool
}

// NewParser 创建语法分析器并绑定词法器。escapeFunc 传 nil 时用默认的
// NevextEscape；Reset 同理。
func NewParser(lexer *Lexer, escapeFunc ...EscapeFunc) *Parser {
	p := &Parser{}
	p.Reset(lexer, escapeFunc...)
	return p
}

// Reset 重绑词法器并清空上一轮的错误记录，复用同一对象。
// 可选的 escapeFunc 用于替换转义函数；不传则保持当前值不变。
func (p *Parser) Reset(lexer *Lexer, escapeFunc ...EscapeFunc) {
	p.Lexer = lexer
	if len(escapeFunc) > 0 && escapeFunc[0] != nil {
		p.EscapeFunc = escapeFunc[0]
	} else if p.EscapeFunc == nil {
		p.EscapeFunc = NevextEscape
	}
	p.Errors = nil
	p.token = Token{}
	p.failed = false
}

// ParseProgram 解析整个程序。
// pauseWhenError 为 true（默认传 true）时遇到第一个错误立即停止；
// 无论哪种模式，出错前已经解析出的语句都会保留在返回值里。
func (p *Parser) ParseProgram(pauseWhenError bool) ParseResult {
	result := ParseResult{Err: Success, Val: []Statement{}}
	p.failed = false
	for {
		p.token = p.Lexer.NextToken()
		switch p.token.Type {
		case EndOfFile:
			// 最后一个语句可以没有 ; 结尾，直接退出循环
		case Sep:
			// 空语句，不处理
		case RawStr, QuotedStr:
			stmt := Statement{Line: p.token.Line, Strings: []string{}}
			stmt.Strings = append(stmt.Strings, p.toLiteral(p.token))
			p.scanRestOfStatement(&stmt)
			result.Val = append(result.Val, stmt)
		default:
			// 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
			// 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
			p.unexpectedTokenError()
		}
		if p.token.Type == EndOfFile {
			break
		}
		if pauseWhenError && len(p.Errors) > 0 {
			break
		}
	}
	if p.failed {
		result.Err = Error
	}
	return result
}

// ------------------------------------------------------------- 私有方法

// scanRestOfStatement 语句内部扫描：直到分号 / EOF / 非法 token。
func (p *Parser) scanRestOfStatement(stmt *Statement) {
	for {
		p.token = p.Lexer.NextToken()
		switch p.token.Type {
		case EndOfFile:
			return // 最后一个语句可以没有 ; 结尾
		case Unexpected:
			p.unexpectedTokenError()
			return
		case RawStr, QuotedStr:
			stmt.Strings = append(stmt.Strings, p.toLiteral(p.token))
		case Sep:
			return
		default:
			p.undefinedTokenTypeError()
			return
		}
	}
}

// toLiteral 只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。
func (p *Parser) toLiteral(token Token) string {
	if token.Type != QuotedStr {
		return token.Literal
	}
	err, val, reason := p.EscapeFunc(token.Literal)
	if err != Success {
		msg := InvalidEscapeMessage
		if reason == "unclosed" {
			msg = UnclosedMessage
		}
		p.Errors = append(p.Errors, "line "+strconv.Itoa(token.Line)+": "+msg)
		p.failed = true
	}
	return val
}

func (p *Parser) unexpectedTokenError() {
	p.Errors = append(p.Errors, "line "+strconv.Itoa(p.token.Line)+": "+UnexpectedMessage)
	p.failed = true
}

func (p *Parser) undefinedTokenTypeError() {
	// TokenType 只有 5 个取值且全部被处理，这里是防御性兜底
	p.Errors = append(p.Errors,
		"exception: line "+strconv.Itoa(p.token.Line)+": undefined token type "+strconv.Itoa(int(p.token.Type)))
	p.failed = true
}
