// 语法分析（Parser）测试
//
// 对应用例编号见 cpp/docs/test-plan.md 的「4. 语法套件 Parser」。
// 覆盖 overview.md 中规定的语句结构、逗号规则、空语句、末语句省略分号、转义表。

#include <string>
#include <vector>

#include "test_framework.hpp"
#include "test_util.hpp"

namespace nevext_test {
namespace {

using Nevext::Result;

// ---------------------------------------------------------------- PAR-01
// overview.md 示例 1：windowSize 1280, 720;
NEVEXT_TEST(Parser, SpecExampleBasic) {
  auto out = Run(u"windowSize 1280, 720;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"windowSize", u"1280", u"720"}}});
}

// ---------------------------------------------------------------- PAR-02
// overview.md 示例 2：连续/多余逗号不影响结果
NEVEXT_TEST(Parser, SpecExampleRedundantCommas) {
  auto out = Run(u"windowSize 1280, , 720 ,;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"windowSize", u"1280", u"720"}}});
}

// ---------------------------------------------------------------- PAR-03
// 空语句不保留（overview.md 细节第 2 条）
NEVEXT_TEST(Parser, EmptyStatementsAreDropped) {
  auto out = Run(u";; a ;;; b ;;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}, {1, {u"b"}}});
}

// ---------------------------------------------------------------- PAR-04
// 最后一个语句可以没有分号（overview.md 细节第 3 条）
NEVEXT_TEST(Parser, LastStatementWithoutSemicolon) {
  auto out = Run(u"a b; c d");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"a", u"b"}}, {1, {u"c", u"d"}}});
}

// ---------------------------------------------------------------- PAR-05
// 只有注释 / 只有空白 / 空输入：结果为空程序且成功
NEVEXT_TEST(Parser, OnlyCommentsAndWhitespace) {
  auto c1 = Run(u"// hi\n/* there */\n");
  ExpectClean(ctx, c1);
  CHECK_EQ(std::size_t(0), c1.prog.size());

  auto c2 = Run(u"  \n\t ");
  ExpectClean(ctx, c2);
  CHECK_EQ(std::size_t(0), c2.prog.size());

  auto c3 = Run(u"");
  ExpectClean(ctx, c3);
  CHECK_EQ(std::size_t(0), c3.prog.size());
}

// ---------------------------------------------------------------- PAR-06
// 完整转义表（overview.md「转义字符」一节全部 9 项）
NEVEXT_TEST(Parser, AllSupportedEscapes) {
  std::u16string src;
  src += u'"';
  src += u'\\'; src += u'\\';  // \\ → 反斜杠
  src += u'\\'; src += u'\'';  // \' → 单引号
  src += u'\\'; src += u'"';   // \" → 双引号
  src += u'\\'; src += u'n';   // \n → 换行
  src += u'\\'; src += u'r';   // \r → 回车
  src += u'\\'; src += u't';   // \t → 制表
  src += u'\\'; src += u'v';   // \v → 垂直制表
  src += u'\\'; src += u'b';   // \b → 退格
  src += u'\\'; src += u'f';   // \f → 换页
  src += u'"';

  auto out = Run(src);
  ExpectClean(ctx, out);

  std::u16string expect;
  expect += u'\\';
  expect += u'\'';
  expect += u'"';
  expect += u'\n';
  expect += u'\r';
  expect += u'\t';
  expect += u'\v';
  expect += u'\b';
  expect += u'\f';

  ExpectProgram(ctx, out.prog, {{1, {expect}}});
  CHECK_EQ(std::size_t(9), expect.size());
}

// ---------------------------------------------------------------- PAR-07
// 裸串不做转义（overview.md：raw_str 不进行转义）
NEVEXT_TEST(Parser, RawStringIsNotUnescaped) {
  std::u16string src;
  src += u'a';
  src += u'\\';
  src += u'n';
  src += u'b';
  src += u';';

  auto out = Run(src);
  ExpectClean(ctx, out);

  std::u16string expect;
  expect += u'a';
  expect += u'\\';
  expect += u'n';
  expect += u'b';
  ExpectProgram(ctx, out.prog, {{1, {expect}}});
}

// ---------------------------------------------------------------- PAR-08
// 同一语句中混用单引号、双引号与裸串
NEVEXT_TEST(Parser, MixedStringKindsInOneStatement) {
  auto out = Run(u"'x' y \"z\";");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"x", u"y", u"z"}}});
}

// ---------------------------------------------------------------- PAR-09
// 空引号串产出空字符串条目，位置不会被跳过
NEVEXT_TEST(Parser, EmptyQuotedStringBecomesEmptyEntry) {
  auto out = Run(u"\"\" '' x;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"", u"", u"x"}}});
}

// ---------------------------------------------------------------- PAR-10
// 语句行号 = 语句第一个 token 所在行
NEVEXT_TEST(Parser, StatementLineNumbers) {
  auto out = Run(u"a;\nb;\n\n c d;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}, {2, {u"b"}}, {4, {u"c", u"d"}}});
}

// ---------------------------------------------------------------- PAR-11
// 非法转义：报错 + 出错语句保留已成功部分 + 返回值 Error
NEVEXT_TEST(Parser, InvalidEscapeReportsError) {
  std::u16string src;
  src += u'"';
  src += u'a';
  src += u'\\';
  src += u'q';
  src += u'b';
  src += u'"';
  src += u';';
  src += u' ';
  src += u'c';

  auto out = Run(src, true);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectErrors(ctx, out.parseErrors, {std::string("line 1: invalid escape character. ")});
  CHECK_EQ(std::size_t(0), out.lexErrors.size());
  // 出错语句保留转义前的部分；默认 pauseWhenError=true，后续语句不再解析
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}});
}

// ---------------------------------------------------------------- PAR-12
// pauseWhenError=false 时继续解析后续语句
NEVEXT_TEST(Parser, ContinueParsingWhenErrorRecoveryEnabled) {
  auto out = Run(u"a;\n\"b\\qc\";\nd;", false);
  CHECK_EQ(ToStr(Result::Error), ToStr(out.result));
  ExpectProgram(ctx, out.prog, {{1, {u"a"}}, {2, {u"b"}}, {3, {u"d"}}});
}

// ---------------------------------------------------------------- PAR-13
// 多行语句：语句之间以分号切分，跨行的字符串列表正确合并
NEVEXT_TEST(Parser, MultiLineStatement) {
  auto out = Run(u"player\n  \"Alice\"\n  100 200;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{1, {u"player", u"Alice", u"100", u"200"}}});
}

// ---------------------------------------------------------------- PAR-14
// 非 ASCII 内容穿越整个管线
NEVEXT_TEST(Parser, NonAsciiRoundTrip) {
  auto out = Run(u"title \"\u6807\u9898\"; note \u6d4b\u8bd5;");
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog,
                {{1, {u"title", u"\u6807\u9898"}}, {1, {u"note", u"\u6d4b\u8bd5"}}});
}

// ---------------------------------------------------------------- PAR-15
// demo/demo01.cpp 的示例输入：与之前的行为保持一致（回归基线）
NEVEXT_TEST(Parser, Demo01InputRegression) {
  std::u16string src = u"\
  // line comment\n\
  hello \"world\";\n\
  /*\n\
  block comment\n\
  */\n\
  a b c";
  auto out = Run(src);
  ExpectClean(ctx, out);
  ExpectProgram(ctx, out.prog, {{2, {u"hello", u"world"}}, {6, {u"a", u"b", u"c"}}});
}

// ---------------------------------------------------------------- PAR-16
// Result 取值约定：Success 为 0，便于调用方写成 if (result)
NEVEXT_TEST(Parser, ResultSuccessIsZero) {
  CHECK_EQ(0, static_cast<int>(Result::Success));
  CHECK_EQ(1, static_cast<int>(Result::Error));

  auto out = Run(u"ok;");
  CHECK_TRUE(out.result == Result::Success);
  CHECK_FALSE(static_cast<bool>(out.result));
}

// ---------------------------------------------------------------- PAR-17
// 跨行引号串之后的语句行号正确（原缺陷 D3 修复后的回归防线）
NEVEXT_TEST(Parser, StatementLineAfterMultiLineQuotedString) {
  auto out = Run(u"\"a\nb\"; c;\nd;");
  ExpectClean(ctx, out);

  std::u16string first;
  first += u'a';
  first += u'\n';
  first += u'b';
  ExpectProgram(ctx, out.prog, {{1, {first}}, {2, {u"c"}}, {3, {u"d"}}});
}

}  // namespace
}  // namespace nevext_test
