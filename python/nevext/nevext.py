# ---------------------------------------------------------------------------
# Nevext 的 Python 实现（Python 3.10+，单文件，零依赖）。
#
# 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
# docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
#   · TokenType 五个枚举项的取值顺序与 C++ 相同；
#   · 词法错误进 lexer.errors，语法/转义错误进 parser.errors，消息文本逐字一致
#     （含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
#   · 未闭合的多行注释不是错误，一直延伸到输入结束；
#   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
#   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
#
# Python 的 str 是 Unicode 码点序列（近似 UTF-32），非 BMP 字符算 1 个元素，
# 而 C++ 的 std::u16string 算 2 个码元；通用语料只断言字符串内容相等，
# 不断言长度/码元数，因此这一差异不影响契约（见文档 X2）。
#
# 与 C++ 的 API 对应关系（Python 惯例 snake_case）：
#   Lexer::NextToken()            -> lexer.next_token()
#   Lexer::Reset(input)           -> lexer.reset(code)
#   Parser::ParseProgram(prog, p) -> parser.parse_program(pause_when_error)
#                                    （语句列表走返回值，不用出参）
#   Parser::Reset(lexer)          -> parser.reset(lexer)
#
# 实现说明：C++/GDScript 每消费一个字符就把缓冲区前移一次（GDScript 因此
# 是超线性开销）；这里改为「完整字符串 + 游标 pos」，行为一致但整体 O(n)。
# ---------------------------------------------------------------------------

from enum import IntEnum

# 库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。
__version__ = "0.4.0"

# 错误消息常量，与 C++ 的消息表逐字一致（含句号后的空格）
UNCLOSED_MESSAGE = "unclosed string literal. "
INVALID_ESCAPE_MESSAGE = "invalid escape character. "
UNEXPECTED_MESSAGE = "unexpected token. "

# 空白与分隔字符集，必须与 C++ 的 whiteChars / rawStrEndChars 逐项一致
WHITE_CHARS = frozenset(" ,\t\v\f\n\r")
RAW_STR_END_CHARS = frozenset(";\"'")


class TokenType(IntEnum):
    """词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（0 起）。"""

    Unexpected = 0  # 非法 token（未闭合引号串、无法推进的字符等）
    EndOfFile = 1  # 输入结束
    RawStr = 2  # 未加引号的裸串（不展开转义）
    QuotedStr = 3  # 引号串（literal 保留转义序列原文，语法层再展开）
    Sep = 4  # 分号


class Result(IntEnum):
    """解析结果码。Success 为 0，可直接写 if res.err。"""

    Success = 0
    Error = 1


class Token:
    """词法单元。literal 为码元原文；EOF / Unexpected 时为空串。"""

    __slots__ = ("type", "line", "literal")

    def __init__(self):
        self.type = TokenType.Unexpected
        self.line = 0
        self.literal = ""


class Statement:
    """一条语句：起始行号 + 解析出的字符串列表。"""

    __slots__ = ("line", "strings")

    def __init__(self):
        self.line = 0
        self.strings = []


class ParseResult:
    """parse_program 的返回值。err 取 Result；val 为语句列表。"""

    __slots__ = ("err", "val")

    def __init__(self):
        self.err = Result.Success
        self.val = []


class Lexer:
    """词法分析器。"""

    def __init__(self, code=""):
        self.reset(code)

    def reset(self, code=""):
        """复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。"""
        self.code = code or ""
        self.pos = 0  # 游标：code 中尚未消费部分的起点
        self.line = 1
        self.errors = []

    def next_token(self):
        """取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。"""
        token = Token()
        self._skip_ws()
        token.line = self.line

        if self.pos >= len(self.code):
            token.type = TokenType.EndOfFile
        elif self.code[self.pos] == ";":
            token.type = TokenType.Sep
            token.literal = ";"
            self.pos += 1
        elif self.code[self.pos] in ("\"", "'"):
            err, val = self._scan_quoted_string(self.code[self.pos])
            if err != Result.Success:
                # 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
                token.type = TokenType.Unexpected
            else:
                token.type = TokenType.QuotedStr
                token.literal = val
        else:
            err, val = self._scan_raw_string()
            if err != Result.Success:
                # 一个字符都无法推进（撞上终止字符），吞掉 1 个字符避免死循环
                token.type = TokenType.Unexpected
                self.pos += 1
            else:
                token.type = TokenType.RawStr
                token.literal = val
        return token

    # ------------------------------------------------------------- 私有方法

    def _scan_quoted_string(self, quote):
        """引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。"""
        self.pos += 1  # 跳过开头的引号
        start = self.pos
        length = 0
        while True:
            if self.pos + length >= len(self.code):
                self._push_error(UNCLOSED_MESSAGE)
                return (Result.Error, "")
            c = self.code[self.pos + length]
            if c == "\\":
                length += 1
                if self.pos + length >= len(self.code):
                    self._push_error(UNCLOSED_MESSAGE)
                    return (Result.Error, "")
                # 被转义的字符若是换行，物理行号同样要推进
                if self.code[self.pos + length] == "\n":
                    self.line += 1
                length += 1
            elif c == quote:
                break
            else:
                if c == "\n":
                    self.line += 1  # 引号串可以跨行，行号必须跟着走
                length += 1  # 普通字符必须推进，否则死循环
        val = self.code[start : start + length]
        self.pos = start + length + 1  # 越过闭合引号
        return (Result.Success, val)

    def _scan_raw_string(self):
        """裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。"""
        start = self.pos
        length = 0
        while self.pos + length < len(self.code):
            c = self.code[self.pos + length]
            if c in WHITE_CHARS or c in RAW_STR_END_CHARS:
                break
            if c == "/" and self.pos + length + 1 < len(self.code):
                nxt = self.code[self.pos + length + 1]
                if nxt == "/" or nxt == "*":
                    break  # 裸串在行注释 / 块注释起始处截断
            length += 1
        if length <= 0:
            return (Result.Error, "")
        self.pos += length
        return (Result.Success, self.code[start : start + length])

    def _skip_ws(self):
        """跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。"""
        while self.pos < len(self.code):
            c = self.code[self.pos]
            if c in WHITE_CHARS:
                if c == "\n":
                    self.line += 1
                self.pos += 1
            elif self._test("//"):
                # 单行注释：跳到行尾（含换行符本身）
                nl = self.code.find("\n", self.pos)
                if nl < 0:
                    self.pos = len(self.code)
                else:
                    self.pos = nl + 1
                    self.line += 1
            elif self._test("/*"):
                self._skip_multiline_comment()
            else:
                break

    def _skip_multiline_comment(self):
        self.pos += 2  # 跳过 /*
        while self.pos < len(self.code):
            if self._test("*/"):
                self.pos += 2
                break
            if self.code[self.pos] == "\n":
                self.line += 1
            self.pos += 1

    def _test(self, form):
        """判断 pos 处是否以 form 开头。"""
        return self.code.startswith(form, self.pos)

    def _push_error(self, msg):
        self.errors.append("line " + str(self.line) + ": " + msg)


# 转义表两套：Nevext 默认表（classic 九项 + \uXXXX，v0.4.0 起）与严格 JSON 表。
# JSON 转义表来自 json.org 语法图（与 ECMA-404 一致）：
# \" \\ \\/ \b \f \n \r \t \uXXXX（4 位十六进制码点，大小写均可；
# 不含 \' 与 \v）。三种转义函数见 null_escape / nevext_escape / json_escape；
# v0.4.0 起 Parser 默认使用 nevext_escape（对 v0.3.0 及之前向后兼容）。
_NEVEXT_ESCAPE_MAP = {
    "\\": "\\", "'": "'", '"': '"',
    "n": "\n", "r": "\r", "t": "\t",
    "v": "\v", "b": "\b", "f": "\f",
}
_JSON_ESCAPE_MAP = {
    '"': '"', "\\": "\\", "/": "/",
    "b": "\b", "f": "\f", "n": "\n",
    "r": "\r", "t": "\t",
}
_JSON_ESCAPE_MAP = {
    '"': '"', "\\": "\\", "/": "/",
    "b": "\b", "f": "\f", "n": "\n",
    "r": "\r", "t": "\t",
}


def null_escape(src):
    """不转义：原样返回（err 恒为 Success）。"""
    return (Result.Success, src, "")


def nevext_escape(src):
    r"""Nevext 默认转义（v0.4.0 起）：classic 九项 + \uXXXX。
    对 v0.3.0 及之前向后兼容：既有转义序列行为不变，只有 \u 从非法变合法。
    返回 (err, val, reason)，失败时 val 保留已展开前缀。"""
    return _escape_with(src, _NEVEXT_ESCAPE_MAP, json_unicode=True)


def json_escape(src):
    r"""严格 JSON 转义：\" \\ \\/ \b \f \n \r \t \uXXXX
    （表来自 json.org 语法图 / ECMA-404；不含 \' 与 \v）。
    \uXXXX 产出对应码点；代理区码点 Python 允许产出（见文档差异登记）。"""
    return _escape_with(src, _JSON_ESCAPE_MAP, json_unicode=True)


def _escape_with(src, escape_map, json_unicode=False):
    """统一展开骨架：失败时 val 保留失败点之前已展开的部分。
    reason 为 "unclosed" | "invalid"，用于错误消息。"""
    out = []
    i = 0
    while i < len(src):
        pos = src.find("\\", i)
        if pos < 0:
            out.append(src[i:])  # 没有转义字符了
            break
        out.append(src[i:pos])
        if pos + 1 >= len(src):
            # 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
            return (Result.Error, "".join(out), "unclosed")
        c = src[pos + 1]
        if json_unicode and c == "u":
            hex4 = src[pos + 2 : pos + 6]
            if len(hex4) != 4:
                return (Result.Error, "".join(out), "invalid")
            try:
                out.append(chr(int(hex4, 16)))
            except ValueError:
                return (Result.Error, "".join(out), "invalid")
            i = pos + 6
            continue
        if c not in escape_map:
            return (Result.Error, "".join(out), "invalid")
        out.append(escape_map[c])
        i = pos + 2
    return (Result.Success, "".join(out), "")


def expand_escapes(src):
    """展开引号串里的转义序列（历史入口，与 Parser 默认一致；
    即 nevext_escape 的同义词）。"""
    return nevext_escape(src)


class Parser:
    """语法分析器。"""

    def __init__(self, lexer=None, escape_func=nevext_escape):
        """escape_func 为转义函数（默认 nevext_escape），可替换为
        json_escape / null_escape 或同签名自定义函数。"""
        self.reset(lexer, escape_func)

    def reset(self, lexer=None, escape_func=None):
        """重绑词法器并清空上一轮的错误记录，复用同一对象。
        escape_func 传 None 时保持当前转义函数不变。"""
        self.lexer = lexer
        if escape_func is not None:
            self.escape_func = escape_func
        self.errors = []
        self._token = None
        self._failed = False

    def parse_program(self, pause_when_error=True):
        """解析整个程序。
        pause_when_error 为 True（默认）时遇到第一个错误立即停止；
        无论哪种模式，出错前已经解析出的语句都会保留在返回值里。"""
        result = ParseResult()
        self._failed = False
        while True:
            self._token = self.lexer.next_token()
            ttype = self._token.type
            if ttype == TokenType.EndOfFile:
                break
            elif ttype == TokenType.Sep:
                pass  # 空语句，不处理
            elif ttype == TokenType.RawStr or ttype == TokenType.QuotedStr:
                stmt = Statement()
                stmt.line = self._token.line
                stmt.strings.append(self._to_literal(self._token))
                self._scan_rest_of_statement(stmt)
                result.val.append(stmt)
            else:
                # 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
                # 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
                self._unexpected_token_error()
            if pause_when_error and self.errors:
                break
        result.err = Result.Error if self._failed else Result.Success
        return result

    # ------------------------------------------------------------- 私有方法

    def _scan_rest_of_statement(self, stmt):
        """语句内部扫描：直到分号 / EOF / 非法 token。"""
        while True:
            self._token = self.lexer.next_token()
            ttype = self._token.type
            if ttype == TokenType.EndOfFile:
                break  # 最后一个语句可以没有 ; 结尾
            elif ttype == TokenType.Unexpected:
                self._unexpected_token_error()
                break
            elif ttype == TokenType.RawStr or ttype == TokenType.QuotedStr:
                stmt.strings.append(self._to_literal(self._token))
            elif ttype == TokenType.Sep:
                break
            else:
                self._undefined_token_type_error()
                break

    def _to_literal(self, token):
        """只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。"""
        if token.type != TokenType.QuotedStr:
            return token.literal
        err, val, reason = self.escape_func(token.literal)
        if err != Result.Success:
            msg = UNCLOSED_MESSAGE if reason == "unclosed" else INVALID_ESCAPE_MESSAGE
            self.errors.append("line " + str(token.line) + ": " + msg)
            self._failed = True
        return val

    def _unexpected_token_error(self):
        self.errors.append("line " + str(self._token.line) + ": " + UNEXPECTED_MESSAGE)
        self._failed = True

    def _undefined_token_type_error(self):
        # TokenType 只有 5 个取值且全部被处理，这里是防御性兜底
        self.errors.append(
            "exception: line "
            + str(self._token.line)
            + ": undefined token type "
            + str(int(self._token.type))
        )
        self._failed = True
