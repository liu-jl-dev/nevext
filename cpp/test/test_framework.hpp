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
// test_framework.hpp —— 极简单元测试框架（header-only，无第三方依赖）
//
// 设计取舍：
//   · 不引入 GTest / Catch2：nevext 核心库是零依赖 header-only，测试也应保持
//     零依赖，一条 g++ 命令就能跑，便于移植与 LLM 生成/改造；
//   · 不使用异常：失败只累加计数并打印，单个断言失败不影响后续用例；
//   · 用例通过静态对象自动注册，新增用例无需修改任何清单；
//   · 诊断信息会把 u16string 转成可读字符串，控制字符转义显示。
//
// 用法：
//   #include "test_framework.hpp"
//
//   NEVEXT_TEST(Lexer, XXX) {          // 用例全名 Lexer.XXX
//     CHECK_EQ(expected, actual);
//     CHECK_TRUE(cond);
//   }
//
//   int main(int argc, char** argv) { return nevext_test::RunAll(argc, argv); }
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common.hpp"

namespace nevext_test {

// ------------------------------------------------------------------ 诊断输出
// UTF-16 → UTF-8（仅用于测试输出的诊断信息，非法代理项降级为 U+FFFD）
inline std::string ToUtf8(std::u16string_view src) {
  std::string out;
  for (std::size_t i = 0; i < src.size();) {
    const char16_t unit = src[i];
    char32_t cp = unit;
    std::size_t step = 1;
    if (unit >= 0xD800 && unit <= 0xDBFF) {  // 高代理项
      if (i + 1 < src.size()) {
        const char16_t low = src[i + 1];
        if (low >= 0xDC00 && low <= 0xDFFF) {
          cp = 0x10000u + (static_cast<char32_t>(unit - 0xD800) << 10) +
               static_cast<char32_t>(low - 0xDC00);
          step = 2;
        } else {
          cp = 0xFFFD;  // 孤立高代理项
        }
      } else {
        cp = 0xFFFD;
      }
    } else if (unit >= 0xDC00 && unit <= 0xDFFF) {  // 孤立低代理项
      cp = 0xFFFD;
    }
    i += step;
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// 把控制字符转成可见转义，避免诊断信息把终端刷乱
inline std::string EscapeVisible(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    switch (c) {
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\v': out += "\\v"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\\': out += "\\\\"; break;
      default:
        if (c < 0x20 || c == 0x7F) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned>(c));
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

inline std::string Show(std::u16string_view s) { return EscapeVisible(ToUtf8(s)); }

// ------------------------------------------------------------ 值的文本化
inline std::string ToStr(int v) { return std::to_string(v); }
inline std::string ToStr(long v) { return std::to_string(v); }
inline std::string ToStr(unsigned long v) { return std::to_string(v); }
inline std::string ToStr(unsigned v) { return std::to_string(v); }
inline std::string ToStr(std::size_t v) { return std::to_string(v); }
inline std::string ToStr(bool v) { return v ? "true" : "false"; }
inline std::string ToStr(const char* v) { return std::string("\"") + EscapeVisible(v) + "\""; }
inline std::string ToStr(const std::string& v) { return std::string("\"") + EscapeVisible(v) + "\""; }
inline std::string ToStr(const char16_t* v) {
  if (v == nullptr) return "(null)";
  return std::string("u\"") + Show(v) + "\"";
}
inline std::string ToStr(std::u16string_view v) { return std::string("u\"") + Show(v) + "\""; }
inline std::string ToStr(const std::u16string& v) { return ToStr(std::u16string_view(v)); }

inline std::string ToStr(Nevext::TokenType t) {
  switch (t) {
    case Nevext::Unexpected: return "Unexpected";
    case Nevext::EndOfFile: return "EndOfFile";
    case Nevext::RawStr: return "RawStr";
    case Nevext::QuotedStr: return "QuotedStr";
    case Nevext::Sep: return "Sep";
    default: return "TokenType(" + std::to_string(static_cast<int>(t)) + ")";
  }
}

inline std::string ToStr(Nevext::Result r) {
  switch (r) {
    case Nevext::Success: return "Success";
    case Nevext::Error: return "Error";
    case Nevext::UnknownError: return "UnknownError";
    case Nevext::UnclosedStringError: return "UnclosedStringError";
    default: return "Result(" + std::to_string(static_cast<int>(r)) + ")";
  }
}

// ------------------------------------------------------------------ 断言实现
struct Context {
  std::string name;
  int checks = 0;
  int failures = 0;
};

inline void ReportFailure(Context& ctx,
                          const char* file,
                          int line,
                          const std::string& detail) {
  ++ctx.failures;
  std::cout << "    [FAIL] " << file << ":" << line << "\n           " << detail
            << "\n";
}

inline void CheckTrueImpl(Context& ctx,
                          bool ok,
                          const char* expr,
                          const char* file,
                          int line) {
  ++ctx.checks;
  if (!ok) {
    ReportFailure(ctx, file, line, std::string("期望为真: ") + expr);
  }
}

template <class Expected, class Actual>
void CheckEqImpl(Context& ctx,
                 const Expected& expected,
                 const Actual& actual,
                 const char* exprExpected,
                 const char* exprActual,
                 const char* file,
                 int line) {
  ++ctx.checks;
  if (!(expected == actual)) {
    ReportFailure(ctx, file, line,
                  std::string("期望 ") + exprExpected + " == " + exprActual +
                      "\n           期望值: " + ToStr(expected) +
                      "\n           实际值: " + ToStr(actual));
  }
}

// -------------------------------------------------------------- 用例注册表
using TestFn = void (*)(Context&);

inline std::vector<std::pair<std::string, TestFn>>& Registry() {
  static std::vector<std::pair<std::string, TestFn>> registry;
  return registry;
}

struct Registrar {
  Registrar(const char* name, TestFn fn) { Registry().emplace_back(name, fn); }
};

// ------------------------------------------------------------------ 执行入口
// 参数：
//   -v                详细模式（打印每个用例）
//   <子串>            只运行名字包含该子串的用例
// 返回值：0 全部通过，1 有失败
inline int RunAll(int argc, char** argv) {
  std::string filter;
  bool verbose = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (!arg.empty()) {
      filter = arg;
    }
  }

  int caseFailed = 0;
  int casePassed = 0;
  int totalChecks = 0;
  int totalFailures = 0;

  std::cout << "==== nevext C++ 单元测试 ====\n";
  for (auto& entry : Registry()) {
    const std::string& name = entry.first;
    if (!filter.empty() && name.find(filter) == std::string::npos) {
      continue;
    }
    Context ctx;
    ctx.name = name;
    std::cout << "[ RUN  ] " << name << "\n";
    entry.second(ctx);
    totalChecks += ctx.checks;
    totalFailures += ctx.failures;
    if (ctx.failures != 0) {
      ++caseFailed;
      std::cout << "[ FAIL ] " << name << "   失败断言 " << ctx.failures << "/"
                << ctx.checks << "\n";
    } else {
      ++casePassed;
      std::cout << (verbose ? "[  OK  ] " : "[  OK  ] ") << name << "   "
                << ctx.checks << " 个断言通过\n";
    }
  }

  std::cout << "--------------------------------------------------\n"
            << "用例: " << (casePassed + caseFailed) << "，通过: " << casePassed
            << "，失败: " << caseFailed << "\n"
            << "断言: " << totalChecks << "，失败: " << totalFailures << "\n";
  if (caseFailed != 0) {
    std::cout << "结果: 失败\n";
    return 1;
  }
  std::cout << "结果: 全部通过\n";
  return 0;
}

}  // namespace nevext_test

// ---------------------------------------------------------------- 用例宏
#define NEVEXT_TEST(suite, name)                                        \
  static void suite##_##name(::nevext_test::Context& ctx);               \
  static ::nevext_test::Registrar suite##_##name##_registrar(            \
      #suite "." #name, &suite##_##name);                                \
  static void suite##_##name(::nevext_test::Context& ctx)

#define CHECK_TRUE(cond) \
  ::nevext_test::CheckTrueImpl(ctx, !!(cond), #cond, __FILE__, __LINE__)
#define CHECK_FALSE(cond) \
  ::nevext_test::CheckTrueImpl(ctx, !(cond), "!(" #cond ")", __FILE__, __LINE__)
#define CHECK_EQ(expected, actual)                                        \
  ::nevext_test::CheckEqImpl(ctx, (expected), (actual), #expected, #actual, \
                             __FILE__, __LINE__)
