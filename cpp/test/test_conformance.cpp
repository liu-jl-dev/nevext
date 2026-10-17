// 跨实现通用语料测试
//
// 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；GDScript 侧见
// gdscript/test/test_conformance.gd。两边必须完全一致：ID、输入、pause 开关、
// 期望的返回值/产物/错误序列。
//
// 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "test_conformance.hpp"
#include "test_util.hpp"

namespace nevext_test {
namespace {

// ------------------------------------------------------------------ 期望值表
// 输入与期望串一律用 C++ 转义书写：\n \r \t \v \b \f \\ \" \'，
// 与文档中「非 ASCII 一律写码元」的约定保持一致，避免源文件编码差异。

using Strs = std::vector<std::u16string>;

StmtExpect St(int line, Strs strings) { return StmtExpect{line, std::move(strings)}; }

ErrExpect Er(int line, const char* kind) { return ErrExpect{line, kind}; }

// clang-format off
const std::vector<ConformanceCase>& Cases() {
  static const std::vector<ConformanceCase> cases = {
    // ---- A 组 程序骨架与语句切分
    {"A01", u"",                        true,  true, {},                                          {}},
    {"A02", u"abc",                     true,  true, {St(1, {u"abc"})},                           {}},
    {"A03", u"a, b\tc",                 true,  true, {St(1, {u"a", u"b", u"c"})},                 {}},
    {"A04", u"a b; c d; e",             true,  true, {St(1, {u"a", u"b"}), St(1, {u"c", u"d"}),
                                                      St(1, {u"e"})},                              {}},
    {"A05", u"a; b",                    true,  true, {St(1, {u"a"}), St(1, {u"b"})},              {}},
    {"A06", u";;;a;",                   true,  true, {St(1, {u"a"})},                             {}},
    {"A07", u"a,,b",                    true,  true, {St(1, {u"a", u"b"})},                       {}},
    {"A08", u"  \t\n ",                 true,  true, {},                                          {}},
    {"A09", u"a b\nc d;",               true,  true, {St(1, {u"a", u"b", u"c", u"d"})},           {}},
    {"A10", u"windowSize 1280, 720;",   true,  true, {St(1, {u"windowSize", u"1280", u"720"})},   {}},

    // ---- B 组 引号串
    {"B01", u"\"hello world\" x",       true,  true, {St(1, {u"hello world", u"x"})},             {}},
    {"B02", u"'abc'",                   true,  true, {St(1, {u"abc"})},                           {}},
    {"B03", u"\"\" z",                  true,  true, {St(1, {u"", u"z"})},                        {}},
    {"B04", u"'a' \"b\" c",             true,  true, {St(1, {u"a", u"b", u"c"})},                 {}},
    {"B05", u"'\"' \"'\"",              true,  true, {St(1, {u"\"", u"'"})},                      {}},
    {"B06", u"\"a\"b",                  true,  true, {St(1, {u"a", u"b"})},                       {}},

    // ---- C 组 转义（只在语法层对 QuotedStr 展开）
    {"C01", u"\"a\\nb\"",               true,  true, {St(1, {u"a\nb"})},                          {}},
    {"C02", u"\"\\n\\r\\t\\v\\b\\f\"",  true,  true, {St(1, {u"\n\r\t\v\b\f"})},                  {}},
    {"C03", u"\"a\\\\\"",               true,  true, {St(1, {u"a\\"})},                           {}},
    {"C04", u"\"ab\\\"cd\"",            true,  true, {St(1, {u"ab\"cd"})},                        {}},
    {"C05", u"'a\\qb'",                 true,  false, {St(1, {u"a"})},                            {Er(1, "InvalidEscape")}},
    {"C06", u"\"a\\qb\"; c",            true,  false, {St(1, {u"a"})},                            {Er(1, "InvalidEscape")}},
    {"C07", u"\"a\\qb\"; c",            false, false, {St(1, {u"a"}), St(1, {u"c"})},             {Er(1, "InvalidEscape")}},
    // X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
    {"C08", u"\"a\\\nb\"",              true,  false, {St(1, {u"a"})},                            {Er(1, "InvalidEscape")}},
    {"C09", u"\"x\\ny\\tz\"",           true,  true, {St(1, {u"x\ny\tz"})},                       {}},

    // ---- D 组 裸串边界（裸串不展开转义）
    {"D01", u"a\\b",                    true,  true, {St(1, {u"a\\b"})},                          {}},
    {"D02", u"a\\nb",                   true,  true, {St(1, {u"a\\nb"})},                         {}},
    {"D03", u"a\\qb",                   true,  true, {St(1, {u"a\\qb"})},                         {}},
    {"D04", u"a\"b",                    true,  false, {St(1, {u"a"})},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    {"D05", u"a\"b",                    false, false, {St(1, {u"a"}), St(1, {u"b"})},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    {"D06", u"a'b",                     false, false, {St(1, {u"a"}), St(1, {u"b"})},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    {"D07", u"a//comment",              true,  true, {St(1, {u"a"})},                             {}},
    {"D08", u"a/**/ b",                 true,  true, {St(1, {u"a", u"b"})},                       {}},
    {"D09", u"\"a//b\"",                true,  true, {St(1, {u"a//b"})},                          {}},
    // 退格不是裸串终止字符：整串是一个 RawStr
    {"D10", u"a" + std::u16string(1, u'\b') + u"b", false, true,
                                        {St(1, {u"a\bb"})},                                       {}},

    // ---- E 组 注释（未闭合的多行注释不是错误）
    {"E01", u"// only comment",         true,  true, {},                                          {}},
    {"E02", u"/* c1\nc2 */ d",          true,  true, {St(2, {u"d"})},                             {}},
    {"E03", u"a /* xx",                 true,  true, {St(1, {u"a"})},                             {}},
    {"E04", u"a /* x\nb; c;",           true,  true, {St(1, {u"a"})},                             {}},
    {"E05", u"/**/a",                   true,  true, {St(1, {u"a"})},                             {}},
    {"E06", u"a //c1\n/* c2 */ b",      true,  true, {St(1, {u"a", u"b"})},                       {}},

    // ---- F 组 行号
    {"F01", u"a;\nb;\nc;",              true,  true, {St(1, {u"a"}), St(2, {u"b"}),
                                                      St(3, {u"c"})},                              {}},
    {"F02", u"a;\r\nb;",                true,  true, {St(1, {u"a"}), St(2, {u"b"})},              {}},
    {"F03", u"\"a\nb\"; c;",            true,  true, {St(1, {u"a\nb"}), St(2, {u"c"})},           {}},
    {"F04", u"\"a\\\nb\"; c;",          true,  false, {St(1, {u"a"})},                            {Er(1, "InvalidEscape")}},
    // 词法错误报在串结束行，语法错误报在串起始行
    {"F05", u"a \"b\nc",                true,  false, {St(1, {u"a"})},
                                        {Er(2, "UnclosedString"), Er(1, "UnexpectedToken")}},

    // ---- G 组 错误与错误恢复
    {"G01", u"a \"bc",                  true,  false, {St(1, {u"a"})},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    {"G02", u"a \"bc",                  false, false, {St(1, {u"a"}), St(1, {u"bc"})},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    // 同一行的多处错误都要记录
    {"G03", u"\"a\\qb\" \"c\\qd\"; e",  false, false, {St(1, {u"a", u"c"}), St(1, {u"e"})},
                                        {Er(1, "InvalidEscape"), Er(1, "InvalidEscape")}},
    {"G04", u"\"abc",                   true,  false, {},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    {"G05", u"\"a\nb",                  true,  false, {},
                                        {Er(2, "UnclosedString"), Er(1, "UnexpectedToken")}},

    // ---- H 组 编码与其它
    {"H01", u"a;\"b\";c",               true,  true, {St(1, {u"a"}), St(1, {u"b"}), St(1, {u"c"})}, {}},
    // 非 BMP 字符：C++ 内部为 UTF-16 代理对，GDScript 为 UTF-32 单码元，内容须相等
    {"H02", u"\"\U0001F600\" x",        true,  true, {St(1, {u"\U0001F600", u"x"})},              {}},
    {"H03", u"\"",                      true,  false, {},
                                        {Er(1, "UnclosedString"), Er(1, "UnexpectedToken")}},
    // 同样为了避免源文件编码差异，非 ASCII 一律写 \uXXXX 码元
    {"H04", u"海上生明月 天涯共此时",
                                        true,  true, {St(1, {u"海上生明月",
                                                             u"天涯共此时"})}, {}},
  };
  return cases;
}
// clang-format on

// ------------------------------------------------------------ 实际结果求值
std::vector<std::string> SemErrors(const Outcome& out) {
  std::vector<std::string> list;
  for (const std::string& e : out.lexErrors) {
    list.push_back(std::to_string(ErrorLine(e)) + ":" + ErrorKind(e));
  }
  for (const std::string& e : out.parseErrors) {
    list.push_back(std::to_string(ErrorLine(e)) + ":" + ErrorKind(e));
  }
  return list;
}

std::vector<std::string> ExpectErrors(const ConformanceCase& c) {
  std::vector<std::string> list;
  for (const ErrExpect& e : c.errs) {
    list.push_back(std::to_string(e.line) + ":" + std::string(e.kind));
  }
  return list;
}

// dump 用：把串内的引号转义掉，保证每行都能被机械解析（Show 已完成其它转义）
std::string QuoteDump(const std::string& s) {
  std::string out;
  for (const char ch : s) {
    if (ch == '"') {
      out += "\\\"";
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

}  // namespace

const std::vector<ConformanceCase>& ConformanceCases() { return Cases(); }

std::string ErrorKind(const std::string& msg) {
  auto has = [&](const char* k) { return msg.find(k) != std::string::npos; };
  if (has("unclosed string literal")) return "UnclosedString";
  if (has("invalid escape character")) return "InvalidEscape";
  if (has("unexpected token")) return "UnexpectedToken";
  return "RAW(" + msg + ")";
}

int ErrorLine(const std::string& msg) {
  const std::size_t p = msg.find("line ");
  if (p == std::string::npos) return -1;
  return std::stoi(msg.substr(p + 5));
}

void CheckConformanceCase(Context& ctx, const char* id) {
  const ConformanceCase* want = nullptr;
  for (const ConformanceCase& c : Cases()) {
    if (std::string(c.id) == id) want = &c;
  }
  if (want == nullptr) {
    ReportFailure(ctx, __FILE__, __LINE__, std::string("语料里没有这条用例: ") + id);
    return;
  }

  std::u16string src = want->src;
  Outcome out = Run(src, want->pause);

  // 1) 返回值
  CHECK_EQ(want->ok ? "Success" : "Error", ToStr(out.result));

  // 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
  CHECK_EQ(want->prog.size(), out.prog.size());
  const std::size_t nstmt = out.prog.size() < want->prog.size() ? out.prog.size()
                                                                : want->prog.size();
  for (std::size_t i = 0; i < nstmt; ++i) {
    CHECK_EQ(want->prog[i].line, out.prog[i].line);
    ExpectStrings(ctx, out.prog[i].strings, want->prog[i].strings);
  }

  // 3) 错误：按 (行号,种类) 序列比较，顺序敏感
  const std::vector<std::string> got = SemErrors(out);
  const std::vector<std::string> exp = ExpectErrors(*want);
  CHECK_EQ(exp.size(), got.size());
  const std::size_t nerr = got.size() < exp.size() ? got.size() : exp.size();
  for (std::size_t i = 0; i < nerr; ++i) {
    CHECK_EQ(exp[i], got[i]);
  }
}

void DumpConformance() {
  for (const ConformanceCase& c : Cases()) {
    std::u16string src = c.src;
    Outcome out = Run(src, c.pause);

    // Show() 已把 \ 转成 \\；在其之后再转义引号才是安全的（否则会把已有的反斜杠吞掉）
    std::string prog;
    for (std::size_t i = 0; i < out.prog.size(); ++i) {
      if (i != 0) prog += "|";
      prog += std::to_string(out.prog[i].line) + ":[";
      for (std::size_t j = 0; j < out.prog[i].strings.size(); ++j) {
        if (j != 0) prog += ",";
        prog += "\"" + QuoteDump(Show(out.prog[i].strings[j])) + "\"";
      }
      prog += "]";
    }

    std::string errs;
    const std::vector<std::string> list = SemErrors(out);
    for (std::size_t i = 0; i < list.size(); ++i) {
      if (i != 0) errs += ",";
      errs += list[i];
    }

    std::cout << c.id << "|" << (out.result == Result::Success ? "OK" : "Err") << "|"
              << prog << "|" << errs << "|pause=" << (c.pause ? "1" : "0") << "\n";
  }
}

}  // namespace nevext_test

// ------------------------------------------------------------------ 用例注册
// 每条语料一个独立用例名，支持 ./nevext_test Conformance.C0 这样的前缀筛选。
#define CONF(id)                                                       \
  NEVEXT_TEST(Conformance, id) { ::nevext_test::CheckConformanceCase(ctx, #id); }

CONF(A01) CONF(A02) CONF(A03) CONF(A04) CONF(A05)
CONF(A06) CONF(A07) CONF(A08) CONF(A09) CONF(A10)
CONF(B01) CONF(B02) CONF(B03) CONF(B04) CONF(B05) CONF(B06)
CONF(C01) CONF(C02) CONF(C03) CONF(C04) CONF(C05)
CONF(C06) CONF(C07) CONF(C08) CONF(C09)
CONF(D01) CONF(D02) CONF(D03) CONF(D04) CONF(D05)
CONF(D06) CONF(D07) CONF(D08) CONF(D09) CONF(D10)
CONF(E01) CONF(E02) CONF(E03) CONF(E04) CONF(E05) CONF(E06)
CONF(F01) CONF(F02) CONF(F03) CONF(F04) CONF(F05)
CONF(G01) CONF(G02) CONF(G03) CONF(G04) CONF(G05)
CONF(H01) CONF(H02) CONF(H03) CONF(H04)

#undef CONF
