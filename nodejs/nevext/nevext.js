// ---------------------------------------------------------------------------
// Nevext 的 JavaScript 实现（Node.js，ESM）。
//
// 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
// docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
//   · TokenType 五个枚举项的取值顺序与 C++ 相同；
//   · 词法错误进 lexer.errors，语法/转义错误进 parser.errors，消息文本逐字一致
//     （含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
//   · 未闭合的多行注释不是错误，一直延伸到输入结束；
//   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
//   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
//
// JS 的字符串就是 UTF-16 码元序列，与 C++ 的 std::u16string 同构，因此
// 代理对、孤立代理项都按码元原样保留，不需要任何编解码处理。
//
// 与 C++ 的 API 对应关系（JS 惯例 camelCase）：
//   Lexer::NextToken()            -> lexer.nextToken()
//   Lexer::Reset(input)           -> lexer.reset(input)
//   Parser::ParseProgram(prog, p) -> parser.parseProgram(pauseWhenError)
//                                    （语句列表走返回值，不用出参）
//   Parser::Reset(lexer)          -> parser.reset(lexer)
//
// 实现 说明：C++/GDScript 每消费一个字符就把缓冲区前移一次（GDScript 因此
// 是超线性开销）；这里改为「完整字符串 + 游标 pos」，行为一致但整体 O(n)。
// ---------------------------------------------------------------------------

/** 库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。 */
export const VERSION = '0.2.0';

/** 词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（Success 式的 0 起）。 */
const TokenType = Object.freeze({
  Unexpected: 0, // 非法 token（未闭合引号串、无法推进的字符等）
  EndOfFile: 1, // 输入结束
  RawStr: 2, // 未加引号的裸串（不展开转义）
  QuotedStr: 3, // 引号串（literal 保留转义序列原文，语法层再展开）
  Sep: 4, // 分号
});

/** 解析结果码。Success 为 0，可直接写 `if (res.err)`。 */
const Result = Object.freeze({
  Success: 0,
  Error: 1,
});

/** 空白与分隔字符集，必须与 C++ 的 whiteChars / rawStrEndChars 逐项一致。 */
const WHITE_CHARS = new Set([' ', ',', '\t', '\v', '\f', '\n', '\r']);
const RAW_STR_END_CHARS = new Set([';', '"', "'"]);

/** 转义表，与 C++ 的 Parser::escapeMap 逐项一致（没有「反斜杠 + 换行」续行）。 */
const ESCAPE_MAP = Object.freeze({
  '\\': '\\',
  "'": "'",
  '"': '"',
  'n': '\n',
  'r': '\r',
  't': '\t',
  'v': '\v',
  'b': '\b',
  'f': '\f',
});

/** 词法单元。literal 为码元原文；EOF / Unexpected 时为空串。 */
class Token {
  constructor() {
    this.type = TokenType.Unexpected;
    this.line = 0;
    this.literal = '';
  }
}

/** 一条语句：起始行号 + 解析出的字符串列表。 */
class Statement {
  constructor() {
    this.line = 0;
    this.strings = [];
  }
}

/** parseProgram 的返回值。err 取 Result；val 为语句列表。 */
class ParseResult {
  constructor() {
    this.err = Result.Success;
    this.val = [];
  }
}

/** 词法分析器。 */
class Lexer {
  constructor(input = '') {
    this.reset(input);
  }

  /** 复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。 */
  reset(input = '') {
    this.code = input;
    this.pos = 0; // 游标：code 中尚未消费部分的起点
    this.line = 1;
    this.errors = [];
  }

  /** 取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。 */
  nextToken() {
    const token = new Token();
    this._skipWs();
    token.line = this.line;

    if (this.pos >= this.code.length) {
      token.type = TokenType.EndOfFile;
    } else if (this.code[this.pos] === ';') {
      token.type = TokenType.Sep;
      token.literal = ';';
      this.pos += 1;
    } else if (this.code[this.pos] === '"' || this.code[this.pos] === "'") {
      const res = this._scanQuotedString(this.code[this.pos]);
      if (res.err !== Result.Success) {
        // 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
        token.type = TokenType.Unexpected;
      } else {
        token.type = TokenType.QuotedStr;
        token.literal = res.val;
      }
    } else {
      const res = this._scanRawString();
      if (res.err !== Result.Success) {
        // 一个字符都无法推进（撞上终止字符），吞掉 1 个字符避免死循环
        token.type = TokenType.Unexpected;
        this.pos += 1;
      } else {
        token.type = TokenType.RawStr;
        token.literal = res.val;
      }
    }
    return token;
  }

  // ------------------------------------------------------------- 私有方法

  /** 引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。 */
  _scanQuotedString(quote) {
    this.pos += 1; // 跳过开头的引号
    const start = this.pos;
    let len = 0;
    while (true) {
      if (this.pos + len >= this.code.length) {
        this._pushError('unclosed string literal. ');
        return { err: Result.Error, val: '' };
      }
      const c = this.code[this.pos + len];
      if (c === '\\') {
        len += 1;
        if (this.pos + len >= this.code.length) {
          this._pushError('unclosed string literal. ');
          return { err: Result.Error, val: '' };
        }
        // 被转义的字符若是换行，物理行号同样要推进
        if (this.code[this.pos + len] === '\n') {
          this.line += 1;
        }
        len += 1;
      } else if (c === quote) {
        break;
      } else {
        if (c === '\n') {
          this.line += 1; // 引号串可以跨行，行号必须跟着走
        }
        len += 1; // 普通字符必须推进，否则死循环
      }
    }
    const val = this.code.slice(start, start + len);
    this.pos = start + len + 1; // 越过闭合引号
    return { err: Result.Success, val };
  }

  /** 裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。 */
  _scanRawString() {
    const start = this.pos;
    let len = 0;
    while (this.pos + len < this.code.length) {
      const c = this.code[this.pos + len];
      if (WHITE_CHARS.has(c) || RAW_STR_END_CHARS.has(c)) {
        break;
      }
      if (c === '/' && this.pos + len + 1 < this.code.length) {
        const next = this.code[this.pos + len + 1];
        if (next === '/' || next === '*') {
          break; // 裸串在行注释 / 块注释起始处截断
        }
      }
      len += 1;
    }
    if (len <= 0) {
      return { err: Result.Error, val: '' };
    }
    this.pos += len;
    return { err: Result.Success, val: this.code.slice(start, start + len) };
  }

  /** 跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。 */
  _skipWs() {
    while (this.pos < this.code.length) {
      const c = this.code[this.pos];
      if (WHITE_CHARS.has(c)) {
        if (c === '\n') {
          this.line += 1;
        }
        this.pos += 1;
      } else if (this._test('//')) {
        // 单行注释：跳到行尾（含换行符本身）
        const nl = this.code.indexOf('\n', this.pos);
        if (nl < 0) {
          this.pos = this.code.length;
        } else {
          this.pos = nl + 1;
          this.line += 1;
        }
      } else if (this._test('/*')) {
        this._skipMultilineComment();
      } else {
        break;
      }
    }
  }

  _skipMultilineComment() {
    this.pos += 2; // 跳过 /*
    while (this.pos < this.code.length) {
      if (this._test('*/')) {
        this.pos += 2;
        break;
      }
      if (this.code[this.pos] === '\n') {
        this.line += 1;
      }
      this.pos += 1;
    }
  }

  /** 判断 pos 处是否以 form 开头。 */
  _test(form) {
    return this.code.startsWith(form, this.pos);
  }

  _pushError(msg) {
    this.errors.push('line ' + this.line + ': ' + msg);
  }
}

/**
 * 展开引号串里的转义序列。与 C++ 的 Parser::escape 一致：
 * 失败时 val 保留失败点之前已展开的部分。
 * 返回 { err, val, reason }，reason 为 'unclosed' | 'invalid'，用于错误消息。
 */
function expandEscapes(src) {
  let out = '';
  let i = 0;
  while (i < src.length) {
    const pos = src.indexOf('\\', i);
    if (pos < 0) {
      out += src.slice(i); // 没有转义字符了
      break;
    }
    out += src.slice(i, pos);
    if (pos + 1 >= src.length) {
      // 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
      return { err: Result.Error, val: out, reason: 'unclosed' };
    }
    const c = src[pos + 1];
    if (!Object.prototype.hasOwnProperty.call(ESCAPE_MAP, c)) {
      return { err: Result.Error, val: out, reason: 'invalid' };
    }
    out += ESCAPE_MAP[c];
    i = pos + 2;
  }
  return { err: Result.Success, val: out, reason: '' };
}

/** 语法分析器。 */
class Parser {
  constructor(lexer = null) {
    this.reset(lexer);
  }

  /** 重绑词法器并清空上一轮的错误记录，复用同一对象。 */
  reset(lexer = null) {
    this.lexer = lexer;
    this.errors = [];
    this._token = null;
    this._failed = false;
  }

  /**
   * 解析整个程序。
   * pauseWhenError 为 true（默认）时遇到第一个错误立即停止；
   * 无论哪种模式，出错前已经解析出的语句都会保留在返回值里。
   */
  parseProgram(pauseWhenError = true) {
    const result = new ParseResult();
    this._failed = false;
    for (;;) {
      this._token = this.lexer.nextToken();
      const type = this._token.type;
      if (type === TokenType.EndOfFile) {
        break;
      } else if (type === TokenType.Sep) {
        // 空语句，不处理
      } else if (type === TokenType.RawStr || type === TokenType.QuotedStr) {
        const stmt = new Statement();
        stmt.line = this._token.line;
        stmt.strings.push(this._toLiteral(this._token));
        this._scanRestOfStatement(stmt);
        result.val.push(stmt);
      } else {
        // 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
        // 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
        this._unexpectedTokenError();
      }
      if (pauseWhenError && this.errors.length > 0) {
        break;
      }
    }
    result.err = this._failed ? Result.Error : Result.Success;
    return result;
  }

  // ------------------------------------------------------------- 私有方法

  /** 语句内部扫描：直到分号 / EOF / 非法 token。 */
  _scanRestOfStatement(stmt) {
    while (true) {
      this._token = this.lexer.nextToken();
      const type = this._token.type;
      if (type === TokenType.EndOfFile) {
        break; // 最后一个语句可以没有 ; 结尾
      } else if (type === TokenType.Unexpected) {
        this._unexpectedTokenError();
        break;
      } else if (type === TokenType.RawStr || type === TokenType.QuotedStr) {
        stmt.strings.push(this._toLiteral(this._token));
      } else if (type === TokenType.Sep) {
        break;
      } else {
        this._undefinedTokenTypeError();
        break;
      }
    }
  }

  /** 只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。 */
  _toLiteral(token) {
    if (token.type !== TokenType.QuotedStr) {
      return token.literal;
    }
    const esc = expandEscapes(token.literal);
    if (esc.err !== Result.Success) {
      const msg =
        esc.reason === 'unclosed'
          ? 'unclosed string literal. '
          : 'invalid escape character. ';
      this.errors.push('line ' + token.line + ': ' + msg);
      this._failed = true;
    }
    return esc.val;
  }

  _unexpectedTokenError() {
    this.errors.push('line ' + this._token.line + ': unexpected token. ');
    this._failed = true;
  }

  _undefinedTokenTypeError() {
    // TokenType 只有 5 个取值且全部被处理，这里是防御性兜底
    this.errors.push(
      'exception: line ' + this._token.line + ': undefined token type ' + this._token.type
    );
    this._failed = true;
  }
}

export {
  TokenType,
  Result,
  Token,
  Statement,
  ParseResult,
  Lexer,
  Parser,
  expandEscapes,
};
