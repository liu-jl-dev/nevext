// 错误处理与健壮性测试
//
// 对应用例编号见 cpp/doc/test-plan.md 的「5. 错误处理与健壮性套件 Error」。
// 本文件中标注了一个「已知行为」（D4），详见文档第 7 节：该用例断言的是当前
// 实现的实际行为，若实现调整需同步更新本文件与文档。

#include <string>
#include <vector>

#include "test_framework.hpp"
#include "test_util.hpp"

namespace nevext_test {
namespace {

using Nevext::Result;
using Nevext::TokenType;

const char* const kUnclosedMsg = "line 1: unclosed string literal. ";
const char* const kUnexpectedMsg = "line 1: unexpected token. ";
const char* const kInvalidEscapeMsg = "line 1: invalid escape character. ";

// ---------------------------------------------------------------- ERR-01
// 未闭合字符串（默认 pauseWhenError=true）：立即停止解析，并且返回 Error
//
// 原缺陷 D1（已修复）：词法错误以 Unexpected token 的形式流到语法层，
// 语法层必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
NEVEXT_TEST(Error, UnclosedStringStopsAndRecordsErrors) {
  std::u16string src;
  src += u'a';
  src += u';';
  src += u' ';
  src += u'"';
  src += u'b';
  src += u'c';

  auto out = Run(src, true);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectErrors(ctx, out.lexErrors, {std::string(kUnclosedMsg)});
  ExpectErrors(ctx, out.parseErrors, {std::string(kUnexpectedMsg)});
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}});
}

// ---------------------------------------------------------------- ERR-02
// 同一输入在 pauseWhenError=false 下：继续解析，把剩余内容降级为裸串
NEVEXT_TEST(Error, UnclosedStringRecoveryWhenNotPaused) {
  std::u16string src;
  src += u'a';
  src += u';';
  src += u' ';
  src += u'"';
  src += u'b';
  src += u'c';
  src += u';';
  src += u' ';
  src += u'd';

  auto out = Run(src, false);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  CHECK_EQ(std::size_t(1), out.lexErrors.size());
  CHECK_EQ(std::size_t(1), out.parseErrors.size());
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}, {1, {u"bc"}}, {1, {u"d"}}});
}

// ---------------------------------------------------------------- ERR-03
// 未闭合的多行注释不报错（overview.md 已明文规定）：注释一直延伸到输入结束，
// 其后的内容全部被当作注释丢弃
NEVEXT_TEST(Error, UnterminatedBlockCommentIsNotAnError) {
  auto out = Run(u"a /* xx", true);
  CHECK_EQ(ToStr(Result::Success), ToStr(out.result));
  CHECK_EQ(std::size_t(0), out.lexErrors.size());
  CHECK_EQ(std::size_t(0), out.parseErrors.size());
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}});

  // 注释之后即使有完整语句，也一并被吞掉，且不报错
  auto swallowed = Run(u"a; /* oops\nb c;\nd;", true);
  CHECK_EQ(ToStr(Result::Success), ToStr(swallowed.result));
  CHECK_EQ(std::size_t(0), swallowed.lexErrors.size());
  CHECK_EQ(std::size_t(0), swallowed.parseErrors.size());
  ExpectProgram(ctx, swallowed.prog, {{1, {u"a"}}});
}

// ---------------------------------------------------------------- ERR-04
// 错误消息格式：「line <n>: <提示> 」，行号取 token 所在行
NEVEXT_TEST(Error, ErrorMessageFormat) {
  auto single = Run(u"\"a", true);
  ExpectErrors(ctx, single.lexErrors, {std::string(kUnclosedMsg)});

  std::u16string multi;
  multi += u'a';
  multi += u';';
  multi += u'\n';
  multi += u'"';
  multi += u'b';
  auto onLine2 = Run(multi, true);
  ExpectErrors(ctx, onLine2.lexErrors, {std::string("line 2: unclosed string literal. ")});
}

// ---------------------------------------------------------------- ERR-05
// 同一行内的多处错误全部上报（旧的按行去重逻辑已移除，见原缺陷 D4）
NEVEXT_TEST(Error, SameLineErrorsAreAllReported) {
  auto out = Run(u"\"a\\qb\" \"c\\qd\"; e", false);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectErrors(ctx, out.parseErrors,
               {std::string(kInvalidEscapeMsg), std::string(kInvalidEscapeMsg)});
}

// ---------------------------------------------------------------- ERR-06
// 不同行的错误各自保留，顺序与源码顺序一致
NEVEXT_TEST(Error, ErrorsOnDifferentLinesAreAllReported) {
  auto out = Run(u"\"a\\qb\";\n\"c\\qd\";\ne;", false);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectErrors(ctx, out.parseErrors,
               {std::string("line 1: invalid escape character. "),
                std::string("line 2: invalid escape character. ")});
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}, {2, {u"c"}}, {3, {u"e"}}});
}

// ---------------------------------------------------------------- ERR-07
// 病态输入必须终止：只有引号、只有反斜杠、混合引号
NEVEXT_TEST(Error, PathologicalInputsTerminate) {
  const std::vector<std::u16string> cases = {
      u"\"",       u"\"\"\"",     u"'",         u"'''",
      u"\\\\\\",  u"\"\\\\",     u"'\\",       u"\"'\"'\"",
      u";;;\"",   u"/*",         u"*/",        u"//",
  };
  for (auto& src : cases) {
    auto tokens = LexAll(src, 512);
    CHECK_FALSE(tokens.empty());
    CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(tokens.back().type));
  }
}

// ---------------------------------------------------------------- ERR-08
// 前缀模糊测试：恶意字符串的所有前缀都必须能扫到 EOF，不死循环、不崩溃
NEVEXT_TEST(Error, EveryPrefixTerminates) {
  const std::u16string nasty = u"a \"b\\\"\\c' /* xx //\ny ;\t'z\"*/ ;";
  for (std::size_t len = 0; len <= nasty.size(); ++len) {
    auto tokens = LexAll(std::u16string(nasty.substr(0, len)), 512);
    CHECK_FALSE(tokens.empty());
    if (tokens.back().type != TokenType::EndOfFile) {
      CHECK_EQ(ToStr(TokenType::EndOfFile), ToStr(tokens.back().type));
      std::cout << "          前缀 len=" << len << ": "
                << ToStr(std::u16string(nasty.substr(0, len))) << "\n";
      break;
    }
  }
}

// ---------------------------------------------------------------- ERR-09
// 大输入：单个裸串 token 不被截断
NEVEXT_TEST(Error, LongRawTokenIsComplete) {
  const std::size_t n = 10000;
  std::u16string src(n, u'x');
  src.push_back(u';');

  auto tokens = LexAll(src, 16);
  CHECK_EQ(std::size_t(3), tokens.size());  // RawStr + Sep + EndOfFile
  CHECK_EQ(ToStr(TokenType::RawStr), ToStr(tokens.at(0).type));
  CHECK_EQ(ToStr(TokenType::Sep), ToStr(tokens.at(1).type));
  CHECK_EQ(n, tokens.at(0).value.size());

  auto out = Run(src);
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {std::u16string(n, u'x')}}});
}

// ---------------------------------------------------------------- ERR-10
// 大量语句：数量正确且每个语句的内容完整
NEVEXT_TEST(Error, ManyStatementsAreAllParsed) {
  const int count = 2000;
  std::u16string src;
  for (int i = 0; i < count; ++i) {
    src += u"s";
    src += static_cast<char16_t>(u'0' + (i % 10));
    src += u";\n";
  }
  auto out = Run(src);
  ExpectClean(ctx, out);
  CHECK_EQ(static_cast<std::size_t>(count), out.prog.size());
  CHECK_EQ(1, static_cast<int>(out.prog.at(count - 1).strings.size()));
  CHECK_EQ(count, out.prog.at(count - 1).line);
}

// ---------------------------------------------------------------- ERR-11
// Token::value 是指向输入缓冲区的视图：只要缓冲区存活，早先 token 依然有效
NEVEXT_TEST(Error, TokenValuesRemainValidWhileBufferAlive) {
  std::u16string src = u"alpha beta; gamma";
  Nevext::Lexer lexer(src);

  std::vector<Nevext::Token> tokens;
  for (int i = 0; i < 8; ++i) {
    tokens.push_back(lexer.NextToken());
    if (tokens.back().type == TokenType::EndOfFile) break;
  }
  CHECK_EQ(std::size_t(5), tokens.size());
  CHECK_EQ(u"alpha", tokens.at(0).value);
  CHECK_EQ(u"beta", tokens.at(1).value);
  CHECK_EQ(u"gamma", tokens.at(3).value);
}

// ---------------------------------------------------------------- ERR-12
// 跨行未闭合字符串：报错行号取字符串结束（也是文件末尾）所在行
// （原缺陷 D3 修复后，引号串内部的行号会被正确统计）
NEVEXT_TEST(Error, UnclosedStringSpanningLinesReportsLastLine) {
  std::u16string src;
  src += u'"';
  src += u'a';
  src += u'\n';
  src += u'b';

  auto out = Run(src, true);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectErrors(ctx, out.lexErrors, {std::string("line 2: unclosed string literal. ")});
  ExpectErrors(ctx, out.parseErrors, {std::string("line 1: unexpected token. ")});
}

}  // namespace
}  // namespace nevext_test
