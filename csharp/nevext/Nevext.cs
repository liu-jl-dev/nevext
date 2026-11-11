// ---------------------------------------------------------------------------
// Nevext 的 C# 实现（.NET，单文件）。
//
// 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
// docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
//   · TokenType 五个枚举项的取值顺序与 C++ 相同；
//   · 词法错误进 lexer.Errors，语法/转义错误进 parser.Errors，消息文本逐字一致
//     （含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
//   · 未闭合的多行注释不是错误，一直延伸到输入结束；
//   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
//   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
//
// C# 的 string 就是 UTF-16 码元序列，与 C++ 的 std::u16string 同构，因此
// 代理对、孤立代理项都按码元原样保留，不需要任何编解码处理。
//
// 与 C++ 的 API 对应关系（C# 惯例 PascalCase）：
//   Lexer::NextToken()            -> lexer.NextToken()
//   Lexer::Reset(input)           -> lexer.Reset(input)
//   Parser::ParseProgram(prog, p) -> parser.ParseProgram(pauseWhenError)
//                                    （语句列表走返回值，不用出参）
//   Parser::Reset(lexer)          -> parser.Reset(lexer)
//
// 实现说明：C++/GDScript 每消费一个字符就把缓冲区前移一次（GDScript 因此
// 是超线性开销）；这里改为「完整字符串 + 游标 _pos」，行为一致但整体 O(n)。
// ---------------------------------------------------------------------------

using System.Collections.Generic;

namespace Nevext
{
    /// <summary>库的版本信息。</summary>
    public static class Info
    {
        /// <summary>库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。</summary>
        public const string Version = "0.4.0";
    }

    /// <summary>词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（0 起）。</summary>
    public enum TokenType
    {
        Unexpected = 0, // 非法 token（未闭合引号串、无法推进的字符等）
        EndOfFile = 1,  // 输入结束
        RawStr = 2,     // 未加引号的裸串（不展开转义）
        QuotedStr = 3,  // 引号串（Literal 保留转义序列原文，语法层再展开）
        Sep = 4,        // 分号
    }

    /// <summary>解析结果码。Success 为 0，可直接写 if (res.Err == Result.Error)。</summary>
    public enum Result
    {
        Success = 0,
        Error = 1,
    }

    /// <summary>词法单元。Literal 为码元原文；EOF / Unexpected 时为空串。</summary>
    public sealed class Token
    {
        public TokenType Type { get; set; } = TokenType.Unexpected;
        public int Line { get; set; }
        public string Literal { get; set; } = "";
    }

    /// <summary>一条语句：起始行号 + 解析出的字符串列表。</summary>
    public sealed class Statement
    {
        public int Line { get; set; }
        public List<string> Strings { get; } = new List<string>();
    }

    /// <summary>ParseProgram 的返回值。Err 取 Result；Val 为语句列表。</summary>
    public sealed class ParseResult
    {
        public Result Err { get; set; } = Result.Success;
        public List<Statement> Val { get; } = new List<Statement>();
    }

    /// <summary>词法分析器。</summary>
    public sealed class Lexer
    {
        // 错误消息常量，与 C++ 的消息表逐字一致（含句号后的空格）
        public const string UnclosedMessage = "unclosed string literal. ";
        public const string InvalidEscapeMessage = "invalid escape character. ";
        public const string UnexpectedMessage = "unexpected token. ";

        public int Line { get; private set; } = 1;
        public List<string> Errors { get; private set; } = new List<string>();

        private string _code = "";
        private int _pos; // 游标：_code 中尚未消费部分的起点

        // 空白与分隔字符集，必须与 C++ 的 whiteChars / rawStrEndChars 逐项一致
        private static readonly HashSet<char> WhiteChars = new HashSet<char>
        {
            ' ', ',', '\t', '\v', '\f', '\n', '\r',
        };
        private static readonly HashSet<char> RawStrEndChars = new HashSet<char>
        {
            ';', '"', '\'',
        };

        public Lexer(string input = "")
        {
            Reset(input);
        }

        /// <summary>复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。</summary>
        public void Reset(string input = "")
        {
            _code = input ?? "";
            _pos = 0;
            Line = 1;
            Errors = new List<string>();
        }

        /// <summary>取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。</summary>
        public Token NextToken()
        {
            var token = new Token();
            SkipWs();
            token.Line = Line;

            if (_pos >= _code.Length)
            {
                token.Type = TokenType.EndOfFile;
            }
            else if (_code[_pos] == ';')
            {
                token.Type = TokenType.Sep;
                token.Literal = ";";
                _pos += 1;
            }
            else if (_code[_pos] == '"' || _code[_pos] == '\'')
            {
                var res = ScanQuotedString(_code[_pos]);
                if (res.Err != Result.Success)
                {
                    // 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
                    token.Type = TokenType.Unexpected;
                }
                else
                {
                    token.Type = TokenType.QuotedStr;
                    token.Literal = res.Val;
                }
            }
            else
            {
                var res = ScanRawString();
                if (res.Err != Result.Success)
                {
                    // 一个字符都无法推进（撞上终止字符），吞掉 1 个字符避免死循环
                    token.Type = TokenType.Unexpected;
                    _pos += 1;
                }
                else
                {
                    token.Type = TokenType.RawStr;
                    token.Literal = res.Val;
                }
            }
            return token;
        }

        // ------------------------------------------------------------- 私有方法

        /// <summary>引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。</summary>
        private (Result Err, string Val) ScanQuotedString(char quote)
        {
            _pos += 1; // 跳过开头的引号
            int start = _pos;
            int len = 0;
            while (true)
            {
                if (_pos + len >= _code.Length)
                {
                    PushError(UnclosedMessage);
                    return (Result.Error, "");
                }
                char c = _code[_pos + len];
                if (c == '\\')
                {
                    len += 1;
                    if (_pos + len >= _code.Length)
                    {
                        PushError(UnclosedMessage);
                        return (Result.Error, "");
                    }
                    // 被转义的字符若是换行，物理行号同样要推进
                    if (_code[_pos + len] == '\n')
                    {
                        Line += 1;
                    }
                    len += 1;
                }
                else if (c == quote)
                {
                    break;
                }
                else
                {
                    if (c == '\n')
                    {
                        Line += 1; // 引号串可以跨行，行号必须跟着走
                    }
                    len += 1; // 普通字符必须推进，否则死循环
                }
            }
            string val = _code.Substring(start, len);
            _pos = start + len + 1; // 越过闭合引号
            return (Result.Success, val);
        }

        /// <summary>裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。</summary>
        private (Result Err, string Val) ScanRawString()
        {
            int start = _pos;
            int len = 0;
            while (_pos + len < _code.Length)
            {
                char c = _code[_pos + len];
                if (WhiteChars.Contains(c) || RawStrEndChars.Contains(c))
                {
                    break;
                }
                if (c == '/' && _pos + len + 1 < _code.Length)
                {
                    char next = _code[_pos + len + 1];
                    if (next == '/' || next == '*')
                    {
                        break; // 裸串在行注释 / 块注释起始处截断
                    }
                }
                len += 1;
            }
            if (len <= 0)
            {
                return (Result.Error, "");
            }
            _pos += len;
            return (Result.Success, _code.Substring(start, len));
        }

        /// <summary>跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。</summary>
        private void SkipWs()
        {
            while (_pos < _code.Length)
            {
                char c = _code[_pos];
                if (WhiteChars.Contains(c))
                {
                    if (c == '\n')
                    {
                        Line += 1;
                    }
                    _pos += 1;
                }
                else if (Test("//"))
                {
                    // 单行注释：跳到行尾（含换行符本身）
                    int nl = _code.IndexOf('\n', _pos);
                    if (nl < 0)
                    {
                        _pos = _code.Length;
                    }
                    else
                    {
                        _pos = nl + 1;
                        Line += 1;
                    }
                }
                else if (Test("/*"))
                {
                    SkipMultilineComment();
                }
                else
                {
                    break;
                }
            }
        }

        private void SkipMultilineComment()
        {
            _pos += 2; // 跳过 /*
            while (_pos < _code.Length)
            {
                if (Test("*/"))
                {
                    _pos += 2;
                    break;
                }
                if (_code[_pos] == '\n')
                {
                    Line += 1;
                }
                _pos += 1;
            }
        }

        /// <summary>判断 _pos 处是否以 form 开头。</summary>
        private bool Test(string form)
        {
            return _code.IndexOf(form, _pos, System.StringComparison.Ordinal) == _pos;
        }

        private void PushError(string msg)
        {
            Errors.Add("line " + Line + ": " + msg);
        }
    }

    /// <summary>
    /// 转义函数签名：返回 (Err, Val, Reason)。失败时 Val 保留已展开前缀，
    /// Reason 为 "unclosed" | "invalid"。三个内置实现见
    /// Parser.NullEscape / Parser.NevextEscape / Parser.JsonEscape。
    /// </summary>
    public delegate (Result Err, string Val, string Reason) EscapeFunc(string src);

    /// <summary>语法分析器。</summary>
    public sealed class Parser
    {
        public Lexer Lexer { get; private set; }
        public List<string> Errors { get; private set; } = new List<string>();

        /// <summary>
        /// 转义函数（默认 NevextEscape：classic 九项 + \uXXXX，v0.4.0 起）。可替换为
        /// JsonEscape / NullEscape 或同签名自定义委托。
        /// </summary>
        public EscapeFunc EscapeFunc = NevextEscape;

        private Token _token;
        private bool _failed;

        // 转义表两套：Nevext 默认表（classic 九项 + \uXXXX，v0.4.0 起）与严格 JSON 表。
        // JSON 表来自 json.org 语法图（与 ECMA-404 一致）。见 JsonEscape。
        private static readonly Dictionary<char, char> NevextMap =
            new Dictionary<char, char>
            {
                { '\\', '\\' },
                { '\'', '\'' },
                { '"', '"' },
                { 'n', '\n' },
                { 'r', '\r' },
                { 't', '\t' },
                { 'v', '\v' },
                { 'b', '\b' },
                { 'f', '\f' },
            };

        private static readonly Dictionary<char, char> JsonMap =
            new Dictionary<char, char>
            {
                { '"', '"' },
                { '\\', '\\' },
                { '/', '/' },
                { 'b', '\b' },
                { 'f', '\f' },
                { 'n', '\n' },
                { 'r', '\r' },
                { 't', '\t' },
            };

        public Parser(Lexer lexer)
        {
            Reset(lexer);
        }

        /// <summary>重绑词法器并清空上一轮的错误记录，复用同一对象。</summary>
        public void Reset(Lexer lexer)
        {
            Lexer = lexer;
            Errors = new List<string>();
            _token = null;
            _failed = false;
        }

        /// <summary>
        /// 解析整个程序。
        /// pauseWhenError 为 true（默认）时遇到第一个错误立即停止；
        /// 无论哪种模式，出错前已经解析出的语句都会保留在返回值里。
        /// </summary>
        public ParseResult ParseProgram(bool pauseWhenError = true)
        {
            var result = new ParseResult();
            _failed = false;
            while (true)
            {
                _token = Lexer.NextToken();
                TokenType type = _token.Type;
                if (type == TokenType.EndOfFile)
                {
                    break;
                }
                else if (type == TokenType.Sep)
                {
                    // 空语句，不处理
                }
                else if (type == TokenType.RawStr || type == TokenType.QuotedStr)
                {
                    var stmt = new Statement();
                    stmt.Line = _token.Line;
                    stmt.Strings.Add(ToLiteral(_token));
                    ScanRestOfStatement(stmt);
                    result.Val.Add(stmt);
                }
                else
                {
                    // 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
                    // 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
                    UnexpectedTokenError();
                }
                if (pauseWhenError && Errors.Count > 0)
                {
                    break;
                }
            }
            result.Err = _failed ? Result.Error : Result.Success;
            return result;
        }

        /// <summary>不转义：原样返回（Err 恒为 Success）。</summary>
        public static (Result Err, string Val, string Reason) NullEscape(string src)
        {
            return (Result.Success, src, "");
        }

        /// <summary>Nevext 默认转义（v0.4.0 起）：classic 九项 + \uXXXX。
        /// 对 v0.3.0 及之前向后兼容：既有转义序列行为不变，只有 \u 从非法变合法。
        /// 失败时 Val 保留已展开前缀。</summary>
        public static (Result Err, string Val, string Reason) NevextEscape(string src)
        {
            return EscapeWith(src, NevextMap, true);
        }

        /// <summary>
        /// 严格 JSON 转义：\" \\\\ \\/ \b \f \n \r \t \uXXXX
        /// （转义表来自 json.org 语法图 / ECMA-404；不含 \' 与 \v）；\uXXXX 产出
        /// UTF-16 码元，代理区码点按原码元落盘（见文档差异登记）。
        /// </summary>
        public static (Result Err, string Val, string Reason) JsonEscape(string src)
        {
            return EscapeWith(src, JsonMap, true);
        }

        private static (Result Err, string Val, string Reason) EscapeWith(
            string src, Dictionary<char, char> map, bool jsonUnicode)
        {
            var sb = new System.Text.StringBuilder();
            int i = 0;
            while (i < src.Length)
            {
                int pos = src.IndexOf('\\', i);
                if (pos < 0)
                {
                    sb.Append(src, i, src.Length - i); // 没有转义字符了
                    break;
                }
                sb.Append(src, i, pos - i);
                if (pos + 1 >= src.Length)
                {
                    // 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
                    return (Result.Error, sb.ToString(), "unclosed");
                }
                char c = src[pos + 1];
                if (jsonUnicode && c == 'u')
                {
                    if (pos + 6 > src.Length)
                    {
                        return (Result.Error, sb.ToString(), "invalid");
                    }
                    int cp = 0;
                    for (int k = 1; k <= 4; k++)
                    {
                        int h = src[pos + 1 + k];
                        int v =
                            (h >= '0' && h <= '9') ? h - '0'
                            : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                            : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                            : -1;
                        if (v < 0)
                        {
                            return (Result.Error, sb.ToString(), "invalid");
                        }
                        cp = cp * 16 + v;
                    }
                    sb.Append((char)cp);
                    i = pos + 6;
                    continue;
                }
                if (!map.TryGetValue(c, out char mapped))
                {
                    return (Result.Error, sb.ToString(), "invalid");
                }
                sb.Append(mapped);
                i = pos + 2;
            }
            return (Result.Success, sb.ToString(), "");
        }

        /// <summary>
        /// 展开引号串里的转义序列（历史入口，与 Parser 默认一致；
        /// 即 NevextEscape 的同义词）。
        /// </summary>
        public static (Result Err, string Val, string Reason) ExpandEscapes(string src)
        {
            return NevextEscape(src);
        }

        // ------------------------------------------------------------- 私有方法

        /// <summary>语句内部扫描：直到分号 / EOF / 非法 token。</summary>
        private void ScanRestOfStatement(Statement stmt)
        {
            while (true)
            {
                _token = Lexer.NextToken();
                TokenType type = _token.Type;
                if (type == TokenType.EndOfFile)
                {
                    break; // 最后一个语句可以没有 ; 结尾
                }
                else if (type == TokenType.Unexpected)
                {
                    UnexpectedTokenError();
                    break;
                }
                else if (type == TokenType.RawStr || type == TokenType.QuotedStr)
                {
                    stmt.Strings.Add(ToLiteral(_token));
                }
                else if (type == TokenType.Sep)
                {
                    break;
                }
                else
                {
                    UndefinedTokenTypeError();
                    break;
                }
            }
        }

        /// <summary>只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。</summary>
        private string ToLiteral(Token token)
        {
            if (token.Type != TokenType.QuotedStr)
            {
                return token.Literal;
            }
            var esc = EscapeFunc(token.Literal);
            if (esc.Err != Result.Success)
            {
                string msg =
                    esc.Reason == "unclosed" ? Lexer.UnclosedMessage : Lexer.InvalidEscapeMessage;
                Errors.Add("line " + token.Line + ": " + msg);
                _failed = true;
            }
            return esc.Val;
        }

        private void UnexpectedTokenError()
        {
            Errors.Add("line " + _token.Line + ": " + Lexer.UnexpectedMessage);
            _failed = true;
        }

        private void UndefinedTokenTypeError()
        {
            // TokenType 只有 5 个取值且全部被处理，这里是防御性兜底
            Errors.Add(
                "exception: line " + _token.Line + ": undefined token type " + (int)_token.Type
            );
            _failed = true;
        }
    }
}
