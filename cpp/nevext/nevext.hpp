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

// Nevext 的 C++ 实现：单文件、header-only、零第三方依赖。
//
// 本文件即全部实现，分节顺序就是依赖顺序：
//   类型（枚举 / Token / Statement） → 词法（Lexer） → 语法（Parser）
//
// 另外四套实现（GDScript / JavaScript / C# / Python）各自也是单文件，行为以本
// 文件为准。字符串模型：内部统一 std::u16string / std::u16string_view（UTF-16
// 码元），与外部 UTF-8 的转换由调用方负责（示例见 cpp/demo/utf.hpp）。

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Nevext {

// 库版本。与根目录 CHANGELOG.md、git tag（v 前缀）同步更新；全部落点的
// 一致性由 tools/check_version.py 守护。
inline constexpr std::string_view kVersion = "0.4.0";

// ---------------------------------------------------------------------------
// 类型
// ---------------------------------------------------------------------------

enum TokenType {
  Unexpected,
  EndOfFile,
  RawStr,
  QuotedStr,
  Sep,
};
enum Result {
  Success,
  Error,
  UnknownError,
  UnclosedStringError,
};

// 词法单元。value 是输入缓冲区上的视图，生命周期跟调用方持有的输入串绑定；
// 需要更长存活期请自行拷贝（Parser 正是把裸串直接拷进 Statement 的）。
struct Token {
  int line;
  TokenType type;
  std::u16string_view value;
};

// 一条语句：起始行号 + 展开转义后的字符串列表。
struct Statement {
  int line;
  std::vector<std::u16string> strings;
};
using Program = std::vector<Statement>;

// ---------------------------------------------------------------------------
// 词法
// ---------------------------------------------------------------------------

struct Lexer {
  int line;
  std::u16string_view input;
  std::vector<std::string> errors;
  Lexer(std::u16string& input) { Reset(input); }
  // 复用同一对象解析新的输入：行号、缓冲区视图与错误容器全部回到初始状态，
  // 等价于重新构造一个 Lexer（与 GDScript 实现的 Lexer.reset 对应）。
  void Reset(std::u16string& input) {
    line = 1;
    this->input = input;
    errors.clear();
  }
  Token NextToken() {
    skipWs();
    Token token{};
    token.line = line;
    if (input.empty()) {
      token.type = TokenType::EndOfFile;
    } else if (input[0] == u';') {
      token.type = TokenType::Sep;
      input.remove_prefix(1);
    } else if (input[0] == u'"') {
      size_t length;
      Result res = scanQuotedStr(u'"', length);
      if (res != Result::Success) {
        token.type = TokenType::Unexpected;
      } else {
        token.type = TokenType::QuotedStr;
        token.value = input.substr(0, length);
        input.remove_prefix(length + 1);
      }
    } else if (input[0] == u'\'') {
      size_t length;
      Result res = scanQuotedStr(u'\'', length);
      if (res != Result::Success) {
        token.type = TokenType::Unexpected;
      } else {
        token.type = TokenType::QuotedStr;
        token.value = input.substr(0, length);
        input.remove_prefix(length + 1);
      }
    } else {
      token.type = TokenType::RawStr;
      token.value = scanRawStr();
      if (token.value.empty()) {  // 无法推进，避免死循环
        token.type = TokenType::Unexpected;
        input.remove_prefix(1);
      }
    }
    return token;
  }

 private:
  inline static const std::set<char16_t> whiteChars{
      u' ', u',', u'\t', u'\v', u'\f', u'\n', u'\r',
  };
  bool test(const std::u16string_view& prefix) {
    if (input.size() < prefix.size())
      return false;
    return input.substr(0, prefix.size()) == prefix;
  }
  Result scanQuotedStr(char16_t quote, size_t& length) {
    input.remove_prefix(1);  // 跳过第一个引号
    length = 0;
    for (;;) {
      if (length >= input.size()) {
        error("unclosed string literal. ");
        return Result::UnclosedStringError;
      } else if (input[length] == u'\\') {
        length += 1;
        if (length >= input.size()) {
          error("unclosed string literal. ");
          return Result::UnclosedStringError;
        }
        // 被转义的字符若是换行，物理行号同样要推进
        if (input[length] == u'\n')
          ++line;
        length += 1;
      } else if (input[length] == quote) {
        break;
      } else {
        if (input[length] == u'\n')
          ++line;     // 引号串可以跨行，行号必须跟着走
        length += 1;  // 普通字符，必须推进，否则死循环
      }
    }
    return Result::Success;
  }
  inline static const std::set<char16_t> rawStrEndChars{
      u' ', u',', u'\t', u'\v', u'\f', u'\n', u'\r', u';', u'"', u'\'',
  };
  std::u16string_view scanRawStr() {
    size_t length = 0;
    for (; length < input.size(); ++length) {
      char16_t ch = input[length];
      if (rawStrEndChars.find(ch) != rawStrEndChars.end())
        break;
      if (ch == u'/' && length + 1 < input.size() &&
          (input[length + 1] == u'/' || input[length + 1] == u'*'))
        break;
    }
    std::u16string_view value = input.substr(0, length);
    input.remove_prefix(length);
    return value;
  }
  void skipWs() {
    for (; !input.empty();) {
      if (whiteChars.find(input[0]) != whiteChars.end()) {  // 找到
        if (input[0] == u'\n')
          ++line;
        input.remove_prefix(1);
      } else if (test(u"//")) {  // 单行注释
        size_t pos = input.find(u'\n');
        if (pos == std::u16string_view::npos) {
          input.remove_prefix(input.size());
        } else {
          input.remove_prefix(pos + 1);
          ++line;
        }
      } else if (test(u"/*")) {  // 多行注释
        skipMultilineComment();
      } else {
        break;
      }
    }
  }
  void skipMultilineComment() {
    input.remove_prefix(2);
    for (; !input.empty();) {
      if (test(u"*/")) {
        input.remove_prefix(2);
        break;
      }
      if (input[0] == u'\n') {
        ++line;
      }
      input.remove_prefix(1);
    }
  }
  void error(const char* msg) {
    std::stringstream ss;
    ss << "line " << line << ": " << msg;
    errors.emplace_back(ss.str());
  }
};

// ---------------------------------------------------------------------------
// 转义函数（三种，可通过 Parser::escapeFunc 替换；v0.4.0 起默认 NevextEscape）
//
//   NullEscape     不做任何转义，原样返回
//   NevextEscape   **默认**：classic 九项表（\' \" \\ \n \r \t \v \b \f）
//                  加 \uXXXX（v0.4.0 新增，对 v0.3.0 及之前向后兼容：
//                  既有转义序列行为不变，只有 \u 从非法变合法）
//   JsonEscape     严格 JSON 表：\" \\ \\/ \b \f \n \r \t \uXXXX
//                  （表来自 json.org 语法图，与 ECMA-404 一致；不含 \' 与 \v，
//                  供 JSON 互操作校验场景使用）
//
// 统一签名：把 src 里的转义序列展开写入 dst。失败时置 *reason（取值
// "unclosed" / "invalid"）并返回 Error；dst 保留已展开前缀（与 Parser 的
// 错误恢复语义一致，调用方负责把 reason 映射为错误消息）。
// \uXXXX 产出 UTF-16 码元；代理区码点按原码元落盘，跨实现对拍时
// 请只用非代理码点（Rust / Lua 等按 UTF-8 编码的实现不接受代理，见
// docs/zh-cn/api-reference.md 差异登记）。
// ---------------------------------------------------------------------------

using EscapeFunc = Result (*)(std::u16string_view src, std::u16string& dst,
                              const char** reason);

inline Result NullEscape(std::u16string_view src, std::u16string& dst,
                         const char** reason) {
  (void)reason;  // 永不失败
  dst.clear();
  dst.append(src.data(), src.size());
  return Result::Success;
}

namespace detail {
inline char16_t HexValue(char16_t c) {
  if (c >= u'0' && c <= u'9') return c - u'0';
  if (c >= u'a' && c <= u'f') return c - u'a' + 10;
  if (c >= u'A' && c <= u'F') return c - u'A' + 10;
  return 0xFFFF;  // 非法十六进制位
}

// 统一展开骨架：表项查 map，两者都支持 \uXXXX。
inline Result EscapeWith(std::u16string_view src, std::u16string& dst,
                         const char** reason,
                         const std::map<char16_t, char16_t>& kMap) {
  dst.clear();
  for (;;) {
    size_t pos = src.find(u'\\');
    if (pos == std::u16string_view::npos) {  // 没有转义字符了
      dst.append(src.data(), src.size());
      break;
    }
    dst.append(src.data(), pos);
    src.remove_prefix(pos + 1);
    if (src.size() == 0) {  // 输入串末尾是转义字符
      *reason = "unclosed";
      return Result::Error;
    }
    if (src[0] == u'u') {  // \uXXXX：4 位十六进制码点（大小写均可）
      if (src.size() < 5) {
        *reason = "invalid";
        return Result::Error;
      }
      char16_t cp = 0;
      for (int i = 1; i <= 4; ++i) {
        char16_t h = HexValue(src[i]);
        if (h == 0xFFFF) {
          *reason = "invalid";
          return Result::Error;
        }
        cp = static_cast<char16_t>(cp * 16 + h);
      }
      dst.push_back(cp);
      src.remove_prefix(5);
      continue;
    }
    auto it = kMap.find(src[0]);
    if (it == kMap.end()) {  // 未定义转义
      *reason = "invalid";
      return Result::Error;
    }
    dst.push_back(it->second);
    src.remove_prefix(1);
  }
  return Result::Success;
}
}  // namespace detail

inline Result NevextEscape(std::u16string_view src, std::u16string& dst,
                           const char** reason) {
  static const std::map<char16_t, char16_t> kMap{
      {u'\\', u'\\'}, {u'\'', u'\''}, {u'"', u'"'},
      {u'n', u'\n'},  {u'r', u'\r'},  {u't', u'\t'},
      {u'v', u'\v'},  {u'b', u'\b'},  {u'f', u'\f'},
  };
  return detail::EscapeWith(src, dst, reason, kMap);
}

inline Result JsonEscape(std::u16string_view src, std::u16string& dst,
                         const char** reason) {
  static const std::map<char16_t, char16_t> kMap{
      {u'"', u'"'},  {u'\\', u'\\'}, {u'/', u'/'},
      {u'b', u'\b'}, {u'f', u'\f'}, {u'n', u'\n'},
      {u'r', u'\r'}, {u't', u'\t'},
  };
  return detail::EscapeWith(src, dst, reason, kMap);
}

// ---------------------------------------------------------------------------
// 语法
// ---------------------------------------------------------------------------

struct Parser {
  Lexer* lexer;
  Token curToken;
  std::vector<std::string> errors;
  // 转义函数（公开可替换，默认 NevextEscape：classic 九项 + \uXXXX）。
  // reason 到错误消息的映射（unclosed→unclosed string literal /
  // invalid→invalid escape character）由 Parser 内部完成，与三种内置函数
  // 配套使用；自定义函数同样只需返回 reason，不必自己拼接行号前缀。
  EscapeFunc escapeFunc = NevextEscape;
  Parser(Lexer& lexer) { Reset(lexer); }
  // 复用同一对象解析新的输入：重绑词法器并清空上一轮的错误记录
  // （与 GDScript 实现的 Parser.reset 对应）。curToken 无需手动还原——
  // 下一轮第一次 NextToken 会立即覆盖它。
  void Reset(Lexer& lexer) {
    this->lexer = &lexer;
    errors.clear();
  }
  Result ParseProgram(Program& prog, bool pauseWhenError = true) {
    Result result = Result::Success;
    for (;;) {
      curToken = lexer->NextToken();
      if (curToken.type == TokenType::EndOfFile) {
        break;
      } else if (curToken.type == TokenType::Sep) {
        // 空语句，不处理
      } else if (curToken.type == TokenType::RawStr ||
                 curToken.type == TokenType::QuotedStr) {
        prog.emplace_back(Statement{curToken.line, {}});
        Statement& stmt = prog.back();
        Result res = parseStatement(stmt);
        if (res != Result::Success) {
          result = Result::Error;
        }
      } else {
        // 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
        // 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
        error("unexpected token. ");
        result = Result::Error;
      }
      if (pauseWhenError && errors.size() > 0) {
        break;
      }
    }
    return result;
  }

 private:
  Result parseStatement(Statement& stmt) {
    Result result = Result::Success;
    for (;;) {
      if (curToken.type == TokenType::EndOfFile) {
        break;  // 最后一个语句可以没有 ; 结尾
      } else if (curToken.type == TokenType::Sep) {
        break;  // 不用 NextToken，因为外层每次循环都有
      } else if (curToken.type == TokenType::RawStr) {
        stmt.strings.emplace_back(curToken.value);
      } else if (curToken.type == TokenType::QuotedStr) {
        stmt.strings.emplace_back(u"");
        std::u16string& dst = stmt.strings.back();
        if (escape(curToken.value, dst) != Result::Success) {
          result = Result::Error;
        }
      } else {
        error("unexpected token. ");
        result = Result::Error;
        break;
      }
      curToken = lexer->NextToken();
    }
    return result;
  }
  Result escape(std::u16string_view src, std::u16string& dst) {
    const char* reason = "invalid";
    Result r = escapeFunc(src, dst, &reason);
    if (r != Result::Success) {
      error(std::string_view(reason) == "unclosed" ? "unclosed string literal. "
                                                   : "invalid escape character. ");
    }
    return r;
  }
  void error(const char* msg) {
    std::stringstream ss;
    ss << "line " << curToken.line << ": " << msg;
    errors.emplace_back(ss.str());
  }
};

}  // namespace Nevext
