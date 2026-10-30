// ---------------------------------------------------------------------------
// TestMain.cs —— 测试入口。
//
// 用法（与 cpp/test、gdscript/test、nodejs/test 的入口约定对齐）：
//   dotnet run --project csharp            跑全部用例（Conformance + Impl）
//   ... Conformance.C0                     只跑名字含该子串的用例
//   ... -v                                 详细模式（保留参数兼容）
//   ... --dump                             按 C++ 侧同一行格式打印 55 条语料
//                                          的实际结果，用于跨实现 diff 门禁
// ---------------------------------------------------------------------------

using System;
using System.Text;

namespace NevextTest
{
    internal static class TestMain
    {
        private static int Main(string[] args)
        {
            // dump 里含中文（H04），Windows 控制台默认代码页不是 UTF-8，显式指定
            Console.OutputEncoding = Encoding.UTF8;

            ConformanceSuite.RegisterAll();
            ImplSuite.RegisterAll();

            if (Array.IndexOf(args, "--dump") >= 0)
            {
                ConformanceSuite.DumpConformance();
                return 0;
            }
            return Framework.RunAll(args);
        }
    }
}
