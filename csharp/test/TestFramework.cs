// ---------------------------------------------------------------------------
// TestFramework.cs —— 极简单元测试框架（零第三方依赖）
//
// 设计取舍与 cpp/test/test_framework.hpp 一致：
//   · 不引入任何测试框架：nevext 保持零依赖，dotnet run 一条命令就能跑；
//   · 失败只累加计数并打印，单个断言失败不影响后续用例；
//   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
// ---------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Text;

namespace NevextTest
{
    /// <summary>单个用例的运行上下文：累加断言数与失败数。</summary>
    public sealed class Context
    {
        public string Name;
        public int Checks;
        public int Failures;

        public Context(string name)
        {
            Name = name;
        }
    }

    public static class Framework
    {
        private static readonly List<(string Name, Action<Context> Fn)> Registry =
            new List<(string, Action<Context>)>();

        public static void Register(string name, Action<Context> fn)
        {
            Registry.Add((name, fn));
        }

        /// <summary>把控制字符转成可见转义，避免诊断信息把终端刷乱。
        /// 与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：
        /// 六个具名控制字符用 \n \r \t \v \b \f，反斜杠翻倍，其余控制字符 \xNN。</summary>
        public static string Show(string s)
        {
            var sb = new StringBuilder();
            foreach (char ch in s)
            {
                int unit = ch;
                switch (ch)
                {
                    case '\n':
                        sb.Append("\\n");
                        break;
                    case '\r':
                        sb.Append("\\r");
                        break;
                    case '\t':
                        sb.Append("\\t");
                        break;
                    case '\v':
                        sb.Append("\\v");
                        break;
                    case '\b':
                        sb.Append("\\b");
                        break;
                    case '\f':
                        sb.Append("\\f");
                        break;
                    case '\\':
                        sb.Append("\\\\");
                        break;
                    default:
                        if (unit < 0x20 || unit == 0x7f)
                        {
                            sb.Append("\\x").Append(unit.ToString("x2").ToUpper());
                        }
                        else
                        {
                            sb.Append(ch);
                        }
                        break;
                }
            }
            return sb.ToString();
        }

        private static string Quote(string v)
        {
            return "\"" + Show(v) + "\"";
        }

        private static void ReportFailure(Context ctx, string label, string detail)
        {
            ctx.Failures += 1;
            Console.WriteLine("    [FAIL] " + label + "\n           " + detail);
        }

        public static void CheckTrue(Context ctx, bool cond, string label)
        {
            ctx.Checks += 1;
            if (!cond)
            {
                ReportFailure(ctx, label, "期望为真");
            }
        }

        public static void CheckFalse(Context ctx, bool cond, string label)
        {
            ctx.Checks += 1;
            if (cond)
            {
                ReportFailure(ctx, label, "期望为假");
            }
        }

        public static void CheckEq(Context ctx, object expected, object actual, string label)
        {
            ctx.Checks += 1;
            if (!Equals(expected, actual))
            {
                string expText = expected is string es ? Quote(es) : expected.ToString();
                string actText = actual is string acs ? Quote(acs) : actual.ToString();
                ReportFailure(ctx, label, "期望 " + expText + "，实际 " + actText);
            }
        }

        /// <summary>全部用例跑一遍；argv 支持 &lt;名字子串&gt; 与 -v。返回退出码。</summary>
        public static int RunAll(string[] argv)
        {
            string filter = "";
            foreach (string arg in argv)
            {
                if (arg == "-v" || arg == "--verbose")
                {
                    // 详细模式占位：本框架的输出已经逐用例打印，保留参数只为兼容
                }
                else if (arg != "")
                {
                    filter = arg;
                }
            }

            int caseFailed = 0;
            int casePassed = 0;
            int totalChecks = 0;
            int totalFailures = 0;

            Console.WriteLine("==== nevext C# 单元测试 ====");
            foreach (var (name, fn) in Registry)
            {
                if (filter != "" && !name.Contains(filter))
                {
                    continue;
                }
                var ctx = new Context(name);
                Console.WriteLine("[ RUN  ] " + name);
                try
                {
                    fn(ctx);
                }
                catch (Exception e)
                {
                    ctx.Failures += 1;
                    Console.WriteLine("    [FAIL] 用例抛出异常: " + e);
                }
                totalChecks += ctx.Checks;
                totalFailures += ctx.Failures;
                if (ctx.Failures != 0)
                {
                    caseFailed += 1;
                    Console.WriteLine(
                        "[ FAIL ] " + name + "   失败断言 " + ctx.Failures + "/" + ctx.Checks
                    );
                }
                else
                {
                    casePassed += 1;
                    Console.WriteLine("[  OK  ] " + name + "   " + ctx.Checks + " 个断言通过");
                }
            }

            Console.WriteLine(
                "--------------------------------------------------\n"
                    + "用例: "
                    + (casePassed + caseFailed)
                    + "，通过: "
                    + casePassed
                    + "，失败: "
                    + caseFailed
                    + "\n"
                    + "断言: "
                    + totalChecks
                    + "，失败: "
                    + totalFailures
            );
            if (caseFailed != 0)
            {
                Console.WriteLine("结果: 失败");
                return 1;
            }
            Console.WriteLine("结果: 全部通过");
            return 0;
        }
    }
}
