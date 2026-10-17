// 词法分析（Lexer）测试
//
// 对应用例编号见 cpp/docs/test-plan.md 的「3. 词法套件 Lexer」。
// 用例名形如 LEX-xx，与文档中的表格一一对应。

#include <string>
#include <vector>

#include "test_framework.hpp"
#include "test_util.hpp"

namespace nevext_test {
namespace {

using Nevext::TokenType;

// ---------------------------------------------------------------- LEX-01
// 空输入：应当立刻给出 EndOfFile，不产生任何 token 或错误
NEVEXT_TEST(Lexer, EmptyInputGivesEof) {
  std::u16string src = u"";
  Nevext::Lexer lexer(src);
  Nevext::Token token = lexer.NextToken();

  CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(token.type));
  CHECK_EQ(1, token.line);
  CHECK_TRUE(token.value.empty());
  CHECK_EQ(std::size_t(0), lexer.errors.size());
}

// ---------------------------------------------------------------- LEX-02
// 纯空白（含逗号与各类控制空白）：全部跳过一致 EOF
NEVEXT_TEST(Lexer, WhitespaceAndCommaAreSkipped) {
  auto tokens = LexAll(u" \t\n\v\f\r ,  ");
  CHECK_EQ(std::size_t(1), tokens.size());
  CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(tokens.at(0).type));
}

// ---------------------------------------------------------------- LEX-03
// 裸字符串：连续非特殊字符作为一个 token
NEVEXT_TEST(Lexer, RawStringBasic) {
  auto tokens = LexAll(u"abc");
  ExpectTypeSeq(ctx, tokens, {{1, TokenType::RawStr}, {1, TokenType::EndOfFile}});
  CHECK_EQ(u"abc", tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-04
// 逗号是分隔符（overview.md：逗号等同空白）
NEVEXT_TEST(Lexer, CommaSeparatesRawStrings) {
  auto tokens = LexAll(u"a,b");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
  CHECK_EQ(u"a", tokens.at(0).value);
  CHECK_EQ(u"b", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-05
// 双引号字符串：token 内容不含引号本身
NEVEXT_TEST(Lexer, DoubleQuotedString) {
  auto tokens = LexAll(u"\"hello\"");
  ExpectTypeSeq(ctx, tokens, {{1, TokenType::QuotedStr}, {1, TokenType::EndOfFile}});
  CHECK_EQ(u"hello", tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-06
// 单引号字符串
NEVEXT_TEST(Lexer, SingleQuotedString) {
  auto tokens = LexAll(u"'hello'");
  ExpectTypeSeq(ctx, tokens, {{1, TokenType::QuotedStr}, {1, TokenType::EndOfFile}});
  CHECK_EQ(u"hello", tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-07
// 词法层不做转义：转义序列原样保留交给 Parser 处理
NEVEXT_TEST(Lexer, EscapeSequenceKeptRawByLexer) {
  std::u16string src;
  src += u'"';
  src += u'a';
  src += u'\\';
  src += u'n';
  src += u'b';
  src += u'"';

  auto tokens = LexAll(src);
  ExpectTypeSeq(ctx, tokens, {{1, TokenType::QuotedStr}, {1, TokenType::EndOfFile}});

  std::u16string expect;
  expect += u'a';
  expect += u'\\';
  expect += u'n';
  expect += u'b';
  CHECK_EQ(std::size_t(4), tokens.at(0).value.size());
  CHECK_EQ(expect, tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-08
// 空字符串："" 与 '' 都产出内容为空的 QuotedStr
NEVEXT_TEST(Lexer, EmptyQuotedStrings) {
  auto tokens = LexAll(u"\"\" ''");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {1, TokenType::QuotedStr},
                 {1, TokenType::EndOfFile}});
  CHECK_TRUE(tokens.at(0).value.empty());
  CHECK_TRUE(tokens.at(1).value.empty());
}

// ---------------------------------------------------------------- LEX-09
// 引号内的空格属于内容，不会被当作分隔符
NEVEXT_TEST(Lexer, QuotedStringPreservesSpaces) {
  auto tokens = LexAll(u"\"a b\" c");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
  CHECK_EQ(u"a b", tokens.at(0).value);
  CHECK_EQ(u"c", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-10
// 分号是独立的 Sep token
NEVEXT_TEST(Lexer, SeparatorToken) {
  auto tokens = LexAll(u"a;b");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {1, TokenType::Sep},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
}

// ---------------------------------------------------------------- LEX-11
// 单行注释直到行尾，且换行计入行号
NEVEXT_TEST(Lexer, LineCommentSkippedAndCounted) {
  auto tokens = LexAll(u"// comment\na");
  ExpectTypeSeq(ctx, tokens, {{2, TokenType::RawStr}, {2, TokenType::EndOfFile}});
  CHECK_EQ(u"a", tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-12
// 多行注释内部的换行计入行号
NEVEXT_TEST(Lexer, BlockCommentSkippedAndCounted) {
  auto tokens = LexAll(u"/*\n\n*/ a");
  ExpectTypeSeq(ctx, tokens, {{3, TokenType::RawStr}, {3, TokenType::EndOfFile}});
}

// ---------------------------------------------------------------- LEX-13
// 单行内的多行注释不换行，且注释结束后紧跟的 token 不受影响
NEVEXT_TEST(Lexer, BlockCommentInsideOneLine) {
  auto tokens = LexAll(u"/*x*/a");
  ExpectTypeSeq(ctx, tokens, {{1, TokenType::RawStr}, {1, TokenType::EndOfFile}});
  CHECK_EQ(u"a", tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-14
// CRLF：\r 属于空白，只有 \n 推进行号，故每行只 +1
NEVEXT_TEST(Lexer, CrlfCountsAsOneLine) {
  auto tokens = LexAll(u"a\r\nb");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {2, TokenType::RawStr},
                 {2, TokenType::EndOfFile}});
}

// ---------------------------------------------------------------- LEX-15
// 裸串在行注释起点处结束（注释不被算进 token 内容）
NEVEXT_TEST(Lexer, RawStringStopsAtLineComment) {
  auto tokens = LexAll(u"a//x\nb");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {2, TokenType::RawStr},
                 {2, TokenType::EndOfFile}});
  CHECK_EQ(u"a", tokens.at(0).value);
  CHECK_EQ(u"b", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-16
// 裸串在多行注释起点处结束
NEVEXT_TEST(Lexer, RawStringStopsAtBlockComment) {
  auto tokens = LexAll(u"a/*x*/b");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
  CHECK_EQ(u"a", tokens.at(0).value);
  CHECK_EQ(u"b", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-17
// 相邻 token 无需空白分隔："a"b 是 QuotedStr + RawStr
NEVEXT_TEST(Lexer, AdjacentTokensNeedNoSpace) {
  auto tokens = LexAll(u"\"a\"b");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
}

// ---------------------------------------------------------------- LEX-18
// 引号串内可以出现另一种引号，且无需转义
NEVEXT_TEST(Lexer, OtherQuoteInsideQuotedString) {
  auto tokens = LexAll(u"'\"' \"'\"");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {1, TokenType::QuotedStr},
                 {1, TokenType::EndOfFile}});
  CHECK_EQ(u"\"", tokens.at(0).value);
  CHECK_EQ(u"'", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-19
// 未闭合字符串：产出 Unexpected，记录 unclosed 错误，剩余内容降级为裸串
NEVEXT_TEST(Lexer, UnclosedStringProducesUnexpectedAndRecovers) {
  std::u16string src = u"\"abc";
  std::vector<std::string> errors;
  {
    Nevext::Lexer lexer(src);
    Nevext::Token first = lexer.NextToken();
    CHECK_EQ(ToStr(TokenType::Unexpected), ToStr(first.type));
    CHECK_TRUE(first.value.empty());

    Nevext::Token second = lexer.NextToken();
    CHECK_EQ(ToStr(TokenType::RawStr), ToStr(second.type));
    CHECK_EQ(u"abc", second.value);
    errors = lexer.errors;
  }
  ExpectErrors(ctx, errors, {std::string("line 1: unclosed string literal. ")});
}

// ---------------------------------------------------------------- LEX-20
// 末尾反斜杠吃掉闭合引号，导致字符串未闭合
NEVEXT_TEST(Lexer, TrailingBackslashEscapesClosingQuote) {
  std::u16string src;
  src += u'"';
  src += u'a';
  src += u'b';
  src += u'c';
  src += u'\\';
  src += u'"';

  std::vector<TokenSnapshot> tokens;
  std::vector<std::string> errors;
  {
    Nevext::Lexer lexer(src);
    for (int i = 0; i < 8; ++i) {
      Nevext::Token token = lexer.NextToken();
      tokens.push_back(TokenSnapshot{token.line, token.type, std::u16string(token.value)});
      if (token.type == TokenType::EndOfFile) break;
    }
    errors = lexer.errors;
  }

  CHECK_EQ(std::size_t(4), tokens.size());
  CHECK_EQ(ToStr(TokenType::Unexpected), ToStr(tokens.at(0).type));
  CHECK_EQ(ToStr(TokenType::RawStr), ToStr(tokens.at(1).type));
  CHECK_EQ(ToStr(TokenType::Unexpected), ToStr(tokens.at(2).type));
  CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(tokens.at(3).type));
  CHECK_EQ(std::size_t(2), errors.size());
  for (auto& e : errors) {
    CHECK_EQ(std::string("line 1: unclosed string literal. "), e);
  }
}

// ---------------------------------------------------------------- LEX-21
// 非 ASCII 内容（UTF-16 码元）：中文裸串按 UTF-16 单元原样保留
NEVEXT_TEST(Lexer, NonAsciiRawString) {
  auto tokens = LexAll(u"\u6d77\u4e0a\u751f\u660e\u6708 \u5929\u6daf\u5171\u6b64\u65f6");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::RawStr},
                 {1, TokenType::RawStr},
                 {1, TokenType::EndOfFile}});
  CHECK_EQ(std::size_t(5), tokens.at(0).value.size());
  CHECK_EQ(u"\u6d77\u4e0a\u751f\u660e\u6708", tokens.at(0).value);
  CHECK_EQ(u"\u5929\u6daf\u5171\u6b64\u65f6", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-22
// 只有一个引号：必须报错并且能正常终止（不能死循环）
NEVEXT_TEST(Lexer, LoneQuoteTerminates) {
  auto tokens = LexAll(u"\"");
  CHECK_EQ(std::size_t(2), tokens.size());
  CHECK_EQ(ToStr(TokenType::Unexpected), ToStr(tokens.at(0).type));
  CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(tokens.at(1).type));
}

// ---------------------------------------------------------------- LEX-23
// 多行引号串：内容包含换行，且串内的换行计入行号（原缺陷 D3，已修复）
NEVEXT_TEST(Lexer, QuotedStringCanCrossLines) {
  auto tokens = LexAll(u"\"a\nb\" c");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {2, TokenType::RawStr},
                 {2, TokenType::EndOfFile}});

  std::u16string expect;
  expect += u'a';
  expect += u'\n';
  expect += u'b';
  CHECK_EQ(expect, tokens.at(0).value);
}

// ---------------------------------------------------------------- LEX-24
// 跨行引号串之后的换行继续推进行号：行号不被「吃掉」
NEVEXT_TEST(Lexer, LineCountingContinuesAfterMultiLineQuotedString) {
  auto tokens = LexAll(u"\"a\nb\"\nc;");
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {3, TokenType::RawStr},
                 {3, TokenType::Sep},
                 {3, TokenType::EndOfFile}});
  CHECK_EQ(u"c", tokens.at(1).value);
}

// ---------------------------------------------------------------- LEX-25
// 串内的每个物理换行都计数，包括被反斜杠转义的那个换行
NEVEXT_TEST(Lexer, EscapedNewlineInsideQuotedStringCountsAsLine) {
  std::u16string src;
  src += u'"';
  src += u'a';
  src += u'\\';
  src += u'\n';
  src += u'b';
  src += u'"';
  src += u'\n';
  src += u'c';

  auto tokens = LexAll(src);
  ExpectTypeSeq(ctx, tokens,
                {{1, TokenType::QuotedStr},
                 {3, TokenType::RawStr},
                 {3, TokenType::EndOfFile}});

  std::u16string expect;
  expect += u'a';
  expect += u'\\';
  expect += u'\n';
  expect += u'b';
  CHECK_EQ(expect, tokens.at(0).value);
}

}  // namespace
}  // namespace nevext_test
