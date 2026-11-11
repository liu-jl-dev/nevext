// ---------------------------------------------------------------------------
// TestConformance.cs —— 跨实现通用语料测试（55 条，A01 ~ H04）
//
// 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
// cpp/test/test_conformance.cpp，GDScript 侧见 gdscript/test/test_conformance.gd，
// JavaScript 侧见 nodejs/test/test_conformance.js。
// 各实现必须完全一致：ID、输入、pause 开关、期望的返回值/产物/错误序列。
// 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。
//
// 输入与期望串一律用 C# 转义书写：\n \r \t \v \b \f \\ \" \'，非 ASCII 一律
// 写 \uXXXX 码元（含代理对逐项展开），与文档「避免源文件编码差异」的约定一致。
// ---------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using Nevext;

namespace NevextTest
{
    /// <summary>期望的语句：行号 + 字符串列表。</summary>
    public sealed class StmtExpect
    {
        public readonly int Line;
        public readonly string[] Strings;

        public StmtExpect(int line, string[] strings)
        {
            Line = line;
            Strings = strings;
        }
    }

    /// <summary>期望的错误：行号 + 种类（与消息文本解耦）。</summary>
    public sealed class ErrExpect
    {
        public readonly int Line;
        public readonly string Kind;

        public ErrExpect(int line, string kind)
        {
            Line = line;
            Kind = kind;
        }
    }

    /// <summary>一条通用语料：ID、输入、pause 开关与全部期望值。</summary>
    public sealed class Case
    {
        public readonly string Id;
        public readonly string Src;
        public readonly bool Ok;
        public readonly bool Pause;
        public readonly StmtExpect[] Prog;
        public readonly ErrExpect[] Errs;

        public Case(
            string id,
            string src,
            bool ok,
            bool pause,
            StmtExpect[] prog,
            ErrExpect[] errs
        )
        {
            Id = id;
            Src = src;
            Ok = ok;
            Pause = pause;
            Prog = prog;
            Errs = errs;
        }
    }

    public static class ConformanceSuite
    {
        // ------------------------------------------------------------ 期望值表

        private static StmtExpect[] Prog(params StmtExpect[] stmts)
        {
            return stmts;
        }

        private static ErrExpect[] Errs(params ErrExpect[] errs)
        {
            return errs;
        }

        private static StmtExpect St(int line, params string[] strings)
        {
            return new StmtExpect(line, strings);
        }

        private static ErrExpect Er(int line, string kind)
        {
            return new ErrExpect(line, kind);
        }

        private static readonly Case[] Cases =
        {
            // ---- A 组 程序骨架与语句切分
            new Case("A01", "", true, true, Prog(), Errs()),
            new Case("A02", "abc", true, true, Prog(St(1, "abc")), Errs()),
            new Case("A03", "a, b\tc", true, true, Prog(St(1, "a", "b", "c")), Errs()),
            new Case(
                "A04",
                "a b; c d; e",
                true,
                true,
                Prog(St(1, "a", "b"), St(1, "c", "d"), St(1, "e")),
                Errs()
            ),
            new Case("A05", "a; b", true, true, Prog(St(1, "a"), St(1, "b")), Errs()),
            new Case("A06", ";;;a;", true, true, Prog(St(1, "a")), Errs()),
            new Case("A07", "a,,b", true, true, Prog(St(1, "a", "b")), Errs()),
            new Case("A08", "  \t\n ", true, true, Prog(), Errs()),
            new Case("A09", "a b\nc d;", true, true, Prog(St(1, "a", "b", "c", "d")), Errs()),
            new Case(
                "A10",
                "windowSize 1280, 720;",
                true,
                true,
                Prog(St(1, "windowSize", "1280", "720")),
                Errs()
            ),

            // ---- B 组 引号串
            new Case(
                "B01",
                "\"hello world\" x",
                true,
                true,
                Prog(St(1, "hello world", "x")),
                Errs()
            ),
            new Case("B02", "'abc'", true, true, Prog(St(1, "abc")), Errs()),
            new Case("B03", "\"\" z", true, true, Prog(St(1, "", "z")), Errs()),
            new Case(
                "B04",
                "'a' \"b\" c",
                true,
                true,
                Prog(St(1, "a", "b", "c")),
                Errs()
            ),
            new Case("B05", "'\"' \"'\"", true, true, Prog(St(1, "\"", "'")), Errs()),
            new Case("B06", "\"a\"b", true, true, Prog(St(1, "a", "b")), Errs()),

            // ---- C 组 转义（只在语法层对 QuotedStr 展开；v0.4.0 起默认 NevextEscape：
            // classic 九项 + \uXXXX，对 v0.3.0 及之前向后兼容）
            new Case("C01", "\"a\\nb\"", true, true, Prog(St(1, "a\nb")), Errs()),
            new Case(
                "C02",
                "\"\\n\\r\\t\\v\\b\\f\"",
                true,
                true,
                Prog(St(1, "\n\r\t\v\b\f")),
                Errs()
            ),
            new Case("C03", "\"a\\\\\"", true, true, Prog(St(1, "a\\")), Errs()),
            new Case("C04", "\"ab\\\"cd\"", true, true, Prog(St(1, "ab\"cd")), Errs()),
            new Case(
                "C05",
                "'a\\qb'",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "InvalidEscape"))
            ),
            new Case(
                "C06",
                "\"a\\qb\"; c",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "InvalidEscape"))
            ),
            new Case(
                "C07",
                "\"a\\qb\"; c",
                false,
                false,
                Prog(St(1, "a"), St(1, "c")),
                Errs(Er(1, "InvalidEscape"))
            ),
            // X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
            new Case(
                "C08",
                "\"a\\\nb\"",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "InvalidEscape"))
            ),
            new Case("C09", "\"x\\ny\\tz\"", true, true, Prog(St(1, "x\ny\tz")), Errs()),
            // ---- C 组 \u 扩展（v0.4.0；\u 表项与 json.org 语法图 / ECMA-404 一致）
            // \/ 不在默认表（JsonEscape 才有）：报错并保留前缀
            new Case("C10", "\"a\\/b\"", false, true, Prog(St(1, "a")), Errs(Er(1, "InvalidEscape"))),
            new Case(
                "C11",
                "\"\\u4e2d\\u6587\"",
                true,
                true,
                Prog(St(1, "\u4e2d\u6587")),
                Errs()
            ),
            new Case("C12", "\"\\u4E2D\"", true, true, Prog(St(1, "\u4e2d")), Errs()),
            new Case(
                "C13",
                "\"a\\ub\"",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "InvalidEscape"))
            ),
            // \' 在默认表合法（Nevext 两种引号串都需要它；JsonEscape 下才非法）
            new Case("C14", "'a\\'b'", true, true, Prog(St(1, "a'b")), Errs()),

            // ---- D 组 裸串边界（裸串不展开转义）
            new Case("D01", "a\\b", true, true, Prog(St(1, "a\\b")), Errs()),
            new Case("D02", "a\\nb", true, true, Prog(St(1, "a\\nb")), Errs()),
            new Case("D03", "a\\qb", true, true, Prog(St(1, "a\\qb")), Errs()),
            new Case(
                "D04",
                "a\"b",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            new Case(
                "D05",
                "a\"b",
                false,
                false,
                Prog(St(1, "a"), St(1, "b")),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            new Case(
                "D06",
                "a'b",
                false,
                false,
                Prog(St(1, "a"), St(1, "b")),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            new Case("D07", "a//comment", true, true, Prog(St(1, "a")), Errs()),
            new Case("D08", "a/**/ b", true, true, Prog(St(1, "a", "b")), Errs()),
            new Case("D09", "\"a//b\"", true, true, Prog(St(1, "a//b")), Errs()),
            // 退格不是裸串终止字符：整串是一个 RawStr
            new Case("D10", "a\bb", true, false, Prog(St(1, "a\bb")), Errs()),

            // ---- E 组 注释（未闭合的多行注释不是错误）
            new Case("E01", "// only comment", true, true, Prog(), Errs()),
            new Case("E02", "/* c1\nc2 */ d", true, true, Prog(St(2, "d")), Errs()),
            new Case("E03", "a /* xx", true, true, Prog(St(1, "a")), Errs()),
            new Case("E04", "a /* x\nb; c;", true, true, Prog(St(1, "a")), Errs()),
            new Case("E05", "/**/a", true, true, Prog(St(1, "a")), Errs()),
            new Case(
                "E06",
                "a //c1\n/* c2 */ b",
                true,
                true,
                Prog(St(1, "a", "b")),
                Errs()
            ),

            // ---- F 组 行号
            new Case(
                "F01",
                "a;\nb;\nc;",
                true,
                true,
                Prog(St(1, "a"), St(2, "b"), St(3, "c")),
                Errs()
            ),
            new Case(
                "F02",
                "a;\r\nb;",
                true,
                true,
                Prog(St(1, "a"), St(2, "b")),
                Errs()
            ),
            new Case(
                "F03",
                "\"a\nb\"; c;",
                true,
                true,
                Prog(St(1, "a\nb"), St(2, "c")),
                Errs()
            ),
            new Case(
                "F04",
                "\"a\\\nb\"; c;",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "InvalidEscape"))
            ),
            // 词法错误报在串结束行，语法错误报在串起始行
            new Case(
                "F05",
                "a \"b\nc",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(2, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),

            // ---- G 组 错误与错误恢复
            new Case(
                "G01",
                "a \"bc",
                false,
                true,
                Prog(St(1, "a")),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            new Case(
                "G02",
                "a \"bc",
                false,
                false,
                Prog(St(1, "a"), St(1, "bc")),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            // 同一行的多处错误都要记录
            new Case(
                "G03",
                "\"a\\qb\" \"c\\qd\"; e",
                false,
                false,
                Prog(St(1, "a", "c"), St(1, "e")),
                Errs(Er(1, "InvalidEscape"), Er(1, "InvalidEscape"))
            ),
            new Case(
                "G04",
                "\"abc",
                false,
                true,
                Prog(),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            new Case(
                "G05",
                "\"a\nb",
                false,
                true,
                Prog(),
                Errs(Er(2, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),

            // ---- H 组 编码与其它
            new Case(
                "H01",
                "a;\"b\";c",
                true,
                true,
                Prog(St(1, "a"), St(1, "b"), St(1, "c")),
                Errs()
            ),
            // 非 BMP 字符：C++ 内部为 UTF-16 代理对，C# 同为码元序列，内容须相等
            new Case(
                "H02",
                "\"\uD83D\uDE00\" x",
                true,
                true,
                Prog(St(1, "\uD83D\uDE00", "x")),
                Errs()
            ),
            new Case(
                "H03",
                "\"",
                false,
                true,
                Prog(),
                Errs(Er(1, "UnclosedString"), Er(1, "UnexpectedToken"))
            ),
            // 同样为了避免源文件编码差异，非 ASCII 一律写 \uXXXX 码元
            new Case(
                "H04",
                "\u6D77\u4E0A\u751F\u660E\u6708 \u5929\u6DAF\u5171\u6B64\u65F6",
                true,
                true,
                Prog(
                    St(1, "\u6D77\u4E0A\u751F\u660E\u6708", "\u5929\u6DAF\u5171\u6B64\u65F6")
                ),
                Errs()
            ),
        };

        // ------------------------------------------------------------ 驱动逻辑

        /// <summary>一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。</summary>
        public sealed class RunOutput
        {
            public Result Result;
            public List<Statement> Prog;
            public List<string> LexErrors;
            public List<string> ParseErrors;
        }

        public static RunOutput Run(string src, bool pauseWhenError)
        {
            var lexer = new Lexer(src);
            var parser = new Parser(lexer);
            ParseResult res = parser.ParseProgram(pauseWhenError);
            return new RunOutput
            {
                Result = res.Err,
                Prog = res.Val,
                LexErrors = lexer.Errors,
                ParseErrors = parser.Errors,
            };
        }

        /// <summary>错误消息 -&gt; (行号, 种类) 语义序列。词法错误在前，语法错误在后。</summary>
        public static List<string> SemErrors(RunOutput output)
        {
            string KindOf(string msg)
            {
                if (msg.Contains("unclosed string literal"))
                {
                    return "UnclosedString";
                }
                if (msg.Contains("invalid escape character"))
                {
                    return "InvalidEscape";
                }
                if (msg.Contains("unexpected token"))
                {
                    return "UnexpectedToken";
                }
                return "RAW(" + msg + ")";
            }

            int LineOf(string msg)
            {
                int p = msg.IndexOf("line ", StringComparison.Ordinal);
                if (p < 0)
                {
                    return -1;
                }
                int q = p + 5;
                int e = q;
                while (e < msg.Length && char.IsDigit(msg[e]))
                {
                    e += 1;
                }
                return int.Parse(msg.Substring(q, e - q));
            }

            var all = new List<string>(output.LexErrors);
            all.AddRange(output.ParseErrors);
            return all.ConvertAll(e => LineOf(e) + ":" + KindOf(e));
        }

        public static void CheckCase(Context ctx, string id)
        {
            Case want = Array.Find(Cases, c => c.Id == id);
            if (want == null)
            {
                ctx.Failures += 1;
                Console.WriteLine("    [FAIL] 语料里没有这条用例: " + id);
                return;
            }

            RunOutput output = Run(want.Src, want.Pause);

            // 1) 返回值
            Framework.CheckEq(
                ctx,
                want.Ok ? "Success" : "Error",
                output.Result == Result.Success ? "Success" : "Error",
                "返回值"
            );

            // 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
            Framework.CheckEq(ctx, want.Prog.Length, output.Prog.Count, "语句数");
            int n = Math.Min(output.Prog.Count, want.Prog.Length);
            for (int i = 0; i < n; i += 1)
            {
                Framework.CheckEq(ctx, want.Prog[i].Line, output.Prog[i].Line, "语句[" + i + "]行号");
                Framework.CheckEq(
                    ctx,
                    want.Prog[i].Strings.Length,
                    output.Prog[i].Strings.Count,
                    "语句[" + i + "]字符串数"
                );
                int m = Math.Min(output.Prog[i].Strings.Count, want.Prog[i].Strings.Length);
                for (int j = 0; j < m; j += 1)
                {
                    Framework.CheckEq(
                        ctx,
                        want.Prog[i].Strings[j],
                        output.Prog[i].Strings[j],
                        "语句[" + i + "]字符串[" + j + "]"
                    );
                }
            }

            // 3) 错误：按 (行号,种类) 序列比较，顺序敏感
            List<string> got = SemErrors(output);
            string[] exp = Array.ConvertAll(
                want.Errs,
                e => e.Line + ":" + e.Kind
            );
            Framework.CheckEq(ctx, exp.Length, got.Count, "错误数");
            int k = Math.Min(got.Count, exp.Length);
            for (int i = 0; i < k; i += 1)
            {
                Framework.CheckEq(ctx, exp[i], got[i], "错误[" + i + "]");
            }
        }

        // dump 用：Show 已把 \ 转成 \\；在其之后再转义引号才是安全的
        private static string QuoteDump(string s)
        {
            return s.Replace("\"", "\\\"");
        }

        /// <summary>按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。</summary>
        public static void DumpConformance()
        {
            foreach (Case c in Cases)
            {
                RunOutput output = Run(c.Src, c.Pause);
                var parts = new List<string>();
                foreach (Statement stmt in output.Prog)
                {
                    var quoted = new List<string>();
                    foreach (string s in stmt.Strings)
                    {
                        quoted.Add("\"" + QuoteDump(Framework.Show(s)) + "\"");
                    }
                    parts.Add(stmt.Line + ":[" + string.Join(",", quoted) + "]");
                }
                string prog = string.Join("|", parts);
                string errs = string.Join(",", SemErrors(output));
                string ok = output.Result == Result.Success ? "OK" : "Err";
                Console.WriteLine(
                    c.Id + "|" + ok + "|" + prog + "|" + errs + "|pause=" + (c.Pause ? "1" : "0")
                );
            }
        }

        // ------------------------------------------------------------ 用例注册
        // 每条语料一个独立用例名，支持筛选 Conformance.C0 这样的前缀。

        public static void RegisterAll()
        {
            foreach (Case c in Cases)
            {
                string id = c.Id;
                Framework.Register("Conformance." + id, ctx => CheckCase(ctx, id));
            }
        }
    }
}
