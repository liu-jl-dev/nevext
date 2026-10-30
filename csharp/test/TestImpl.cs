// ---------------------------------------------------------------------------
// TestImpl.cs —— C# 实现私有的用例（与 JavaScript 侧 test_impl.js 对应）
//
// 通用语料（TestConformance.cs）各实现共用；本文件放 C# 自己关心的工程性
// 用例：对象复用、终止性、大输入，以及一条 UTF-16 码元保真用例。
// ---------------------------------------------------------------------------

using Nevext;

namespace NevextTest
{
    public static class ImplSuite
    {
        public static void RegisterAll()
        {
            Framework.Register("Impl.ResetAreReusable", ResetAreReusable);
            Framework.Register("Impl.PathologicalInputsTerminate", PathologicalInputsTerminate);
            Framework.Register("Impl.LongRawTokenIsComplete", LongRawTokenIsComplete);
            Framework.Register("Impl.ManyStatementsAreAllParsed", ManyStatementsAreAllParsed);
            Framework.Register("Impl.LoneSurrogatePreserved", LoneSurrogatePreserved);
        }

        // CS-01 Lexer.Reset / Parser.Reset 能让实例重用到另一份源码
        //       （与 GDScript 的 IMP-02、C++ 的 LEX-26 / PAR-18、JS 的 JS-01 对应）
        private static void ResetAreReusable(Context ctx)
        {
            var lexer = new Lexer("a b");
            var parser = new Parser(lexer);
            ParseResult first = parser.ParseProgram(true);
            Framework.CheckEq(ctx, 1, first.Val.Count, "第一次语句数");
            Framework.CheckEq(
                ctx,
                "Success",
                first.Err == Result.Success ? "Success" : "Error",
                "第一次返回值"
            );

            lexer.Reset("c; d");
            parser.Reset(lexer);
            ParseResult second = parser.ParseProgram(true);
            Framework.CheckEq(ctx, 2, second.Val.Count, "第二次语句数");
            if (second.Val.Count >= 2)
            {
                Framework.CheckEq(ctx, "c", second.Val[0].Strings[0], "第 1 条语句");
                Framework.CheckEq(ctx, "d", second.Val[1].Strings[0], "第 2 条语句");
            }
            Framework.CheckEq(ctx, 0, parser.Errors.Count, "Parser 错误数（Reset 应清空）");
            Framework.CheckEq(ctx, 0, lexer.Errors.Count, "Lexer 错误数（Reset 应清空）");
            Framework.CheckEq(ctx, 1, lexer.Line, "行号应回到 1");
        }

        // CS-02 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
        private static void PathologicalInputsTerminate(Context ctx)
        {
            string src = "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;";
            for (int i = 0; i <= src.Length; i += 1)
            {
                var lexer = new Lexer(src.Substring(0, i));
                bool reachedEof = false;
                for (int step = 0; step < 200; step += 1)
                {
                    if (lexer.NextToken().Type == TokenType.EndOfFile)
                    {
                        reachedEof = true;
                        break;
                    }
                }
                Framework.CheckTrue(ctx, reachedEof, "前缀[" + i + "]能在阈值内扫到 EOF");
            }
        }

        // CS-03 长裸串不被截断
        private static void LongRawTokenIsComplete(Context ctx)
        {
            string src = new string('x', 1000) + ";";
            var lexer = new Lexer(src);
            Token token = lexer.NextToken();
            Framework.CheckEq(
                ctx,
                "RawStr",
                token.Type == TokenType.RawStr ? "RawStr" : "其它",
                "token 类型"
            );
            Framework.CheckEq(ctx, 1000, token.Literal.Length, "裸串长度");
        }

        // CS-04 大量语句全部解析（游标实现不应有码元级性能悬崖）
        private static void ManyStatementsAreAllParsed(Context ctx)
        {
            var lines = new string[500];
            for (int i = 0; i < 500; i += 1)
            {
                lines[i] = "s" + i + " a b;";
            }
            string src = string.Join("\n", lines);
            var lexer = new Lexer(src);
            var parser = new Parser(lexer);
            ParseResult res = parser.ParseProgram(true);
            Framework.CheckEq(ctx, 500, res.Val.Count, "语句数");
            Framework.CheckEq(
                ctx,
                "Success",
                res.Err == Result.Success ? "Success" : "Error",
                "返回值"
            );
            Framework.CheckEq(ctx, "s0", res.Val[0].Strings[0], "首语句首串");
            Framework.CheckEq(ctx, "s499", res.Val[499].Strings[0], "末语句首串");
            Framework.CheckEq(ctx, 500, res.Val[499].Line, "末语句行号");
        }

        // CS-05 孤立代理项按码元原样保留（C++ 同样保留；GDScript 的 UTF-32 无法表达）
        private static void LoneSurrogatePreserved(Context ctx)
        {
            var lexer = new Lexer("x \uD800 y \"\uDFFF\"");
            var parser = new Parser(lexer);
            ParseResult res = parser.ParseProgram(true);
            Framework.CheckEq(ctx, 1, res.Val.Count, "语句数");
            if (res.Val.Count >= 1)
            {
                System.Collections.Generic.List<string> strings = res.Val[0].Strings;
                Framework.CheckEq(ctx, 4, strings.Count, "字符串数");
                if (strings.Count >= 4)
                {
                    Framework.CheckEq(ctx, "\uD800", strings[1], "孤立高代理项原样保留");
                    Framework.CheckEq(ctx, "\uDFFF", strings[3], "孤立低代理项经语法层原样保留");
                }
            }
            Framework.CheckEq(
                ctx,
                0,
                lexer.Errors.Count + parser.Errors.Count,
                "错误数"
            );
        }
    }
}
