#pragma once
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "common.hpp"
// #include "utfcpp/utf8.h"

namespace Nevext {
struct Token {
  int line;
  TokenType type;
  std::u16string_view value;
};
struct Lexer {
  int line;
  std::u16string_view input;
  std::vector<std::string> errors;
  Lexer(std::u16string& input) {
    this->line = 1;
    this->input = input;
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
      u' ', u',', u'\t', u'\v', u'\f', u'\n', u'\r', u'\b',
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
        length += 1;
      } else if (input[length] == quote) {
        break;
      } else {
        length += 1;  // 普通字符，必须推进，否则死循环
      }
    }
    return Result::Success;
  }
  inline static const std::set<char16_t> rawStrEndChars{
      u' ', u',', u'\t', u'\v', u'\f', u'\n', u'\r', u'\b',
      u';', u'"', u'\'',
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
      if (whiteChars.find(input[0]) != whiteChars.end()) { // 找到
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
}  // namespace Nevext
