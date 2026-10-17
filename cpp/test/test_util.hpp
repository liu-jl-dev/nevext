/*******************************************************
MIT License

Copyright (c) 2026 nevext contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 *******************************************************/

#pragma once

// ---------------------------------------------------------------------------
// test_util.hpp —— 测试公共辅助：词法/语法运行器与期望值断言
//
// 之所以要封一层：
//   · Lexer 的构造参数是 std::u16string&，Token::value 是指向该缓冲区的
//     string_view，直接写测试很容易踩悬垂；
//   · Result 返回值与 errors 容器是两套独立的错误通道，每条用例都要同时检查，
//     统一收进 Outcome 避免漏检。
// ---------------------------------------------------------------------------

#include <string>
#include <utility>
#include <vector>

#include "../nevext/nevext.hpp"
#include "test_framework.hpp"

namespace nevext_test {

using Nevext::Program;
using Nevext::Result;
using Nevext::TokenType;

// Token 快照：把 Token::value 拷成独立字符串，脱离输入缓冲区的生命周期
struct TokenSnapshot {
  int line = 0;
  TokenType type = TokenType::Unexpected;
  std::u16string value;
};

// 一次完整解析的结果（返回值 + 产物 + 两侧错误列表）
struct Outcome {
  Result result = Result::Success;
  Program prog;
  std::vector<std::string> lexErrors;
  std::vector<std::string> parseErrors;

  std::size_t errorCount() const { return lexErrors.size() + parseErrors.size(); }
};

// 运行完整管线（Lexer + Parser）。pauseWhenError 对应 ParseProgram 第二参数。
inline Outcome Run(std::u16string source, bool pauseWhenError = true) {
  Outcome out;
  Nevext::Lexer lexer(source);  // source 必须在整个解析期间存活
  Nevext::Parser parser(lexer);
  out.result = parser.ParseProgram(out.prog, pauseWhenError);
  out.lexErrors = lexer.errors;
  out.parseErrors = parser.errors;
  return out;
}

// 只跑词法。limit 是防护阈值：正常输入远在此之前就会遇到 EndOfFile。
inline std::vector<TokenSnapshot> LexAll(std::u16string source, int limit = 256) {
  std::vector<TokenSnapshot> out;
  Nevext::Lexer lexer(source);  // source 必须在整个扫描期间存活
  for (int i = 0; i < limit; ++i) {
    Nevext::Token token = lexer.NextToken();
    out.push_back(TokenSnapshot{token.line, token.type, std::u16string(token.value)});
    if (token.type == TokenType::EndOfFile) {
      break;
    }
  }
  return out;
}

// ------------------------------------------------------------- 期望值断言
// 校验 token 序列的行号与类型（不校验内容，内容由各用例自行 CHECK_EQ）
inline void ExpectTypeSeq(Context& ctx,
                          const std::vector<TokenSnapshot>& actual,
                          const std::vector<std::pair<int, TokenType>>& expect) {
  if (actual.size() != expect.size()) {
    CHECK_EQ(expect.size(), actual.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
      std::cout << "          实际 token[" << i << "]: "
                << ToStr(actual[i].type) << "@" << actual[i].line << " "
                << ToStr(actual[i].value) << "\n";
    }
    return;
  }
  for (std::size_t i = 0; i < expect.size(); ++i) {
    CHECK_EQ(expect[i].first, actual[i].line);
    CHECK_EQ(ToStr(expect[i].second), ToStr(actual[i].type));
  }
}

// 校验单个语句的字符串列表
inline void ExpectStrings(Context& ctx,
                          const std::vector<std::u16string>& actual,
                          const std::vector<std::u16string>& expect) {
  if (actual.size() != expect.size()) {
    CHECK_EQ(expect.size(), actual.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
      std::cout << "          实际 strings[" << i << "]: " << ToStr(actual[i]) << "\n";
    }
    return;
  }
  for (std::size_t i = 0; i < expect.size(); ++i) {
    CHECK_EQ(expect[i], actual[i]);
  }
}

// 校验整个程序的形状：每个元素是一个语句期望的行号 + 字符串列表
struct StmtExpect {
  int line;
  std::vector<std::u16string> strings;
};

inline void ExpectProgram(Context& ctx,
                          const Program& prog,
                          const std::vector<StmtExpect>& expect) {
  CHECK_EQ(expect.size(), prog.size());
  const std::size_t n = prog.size() < expect.size() ? prog.size() : expect.size();
  for (std::size_t i = 0; i < n; ++i) {
    CHECK_EQ(expect[i].line, prog[i].line);
    ExpectStrings(ctx, prog[i].strings, expect[i].strings);
  }
}

// 断言「无任何错误」：既包括返回值，也包括两侧错误容器（见已知缺陷 D1，
// 返回值成功不代表没有错误，所以两边都要查）
inline void ExpectClean(Context& ctx, const Outcome& out) {
  CHECK_EQ(ToStr(Result::Success), ToStr(out.result));
  CHECK_EQ(std::size_t(0), out.lexErrors.size());
  CHECK_EQ(std::size_t(0), out.parseErrors.size());
}

// 断言错误列表与预期完全一致（顺序敏感）
inline void ExpectErrors(Context& ctx,
                         const std::vector<std::string>& actual,
                         const std::vector<std::string>& expect) {
  CHECK_EQ(expect.size(), actual.size());
  const std::size_t n = actual.size() < expect.size() ? actual.size() : expect.size();
  for (std::size_t i = 0; i < n; ++i) {
    CHECK_EQ(expect[i], actual[i]);
  }
}

}  // namespace nevext_test
