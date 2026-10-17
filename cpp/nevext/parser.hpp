#pragma once
#include <map>
#include "lexer.hpp"

namespace Nevext {
struct Statement {
  int line;
  std::vector<std::u16string> strings;
};
using Program = std::vector<Statement>;
struct Parser {
  Lexer* lexer;
  Token curToken;
  std::vector<std::string> errors;
  Parser(Lexer& lexer) : lexer(&lexer) {}
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
  inline static const std::map<char16_t, char16_t> escapeMap{
      {u'\\', u'\\'}, {u'\'', u'\''}, {u'"', u'"'},
      {u'n', u'\n'},  {u'r', u'\r'},  {u't', u'\t'},
      {u'v', u'\v'},  {u'b', u'\b'},  {u'f', u'\f'},
  };
  Result escape(std::u16string_view src, std::u16string& dst) {
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
        error("unclosed string literal. ");
        return Result::Error;
      }
      if (escapeMap.find(src[0]) == escapeMap.end()) {  // 未定义转义
        error("invalid escape character. ");
        return Result::Error;
      }
      dst.push_back(escapeMap.at(src[0]));
      src.remove_prefix(1);
    }
    return Result::Success;
  }
  void error(const char* msg) {
    std::stringstream ss;
    ss << "line " << curToken.line << ": " << msg;
    errors.emplace_back(ss.str());
  }
};
}  // namespace Nevext
