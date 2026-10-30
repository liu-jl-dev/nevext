// ---------------------------------------------------------------------------
// Demo01.cs —— Nevext C# 实现的示例程序。
//
// 运行（需要 .NET SDK）：
//   dotnet run --project csharp/demo             解析内置示例
//   dotnet run --project csharp/demo -- 某文件.txt  解析该文件（UTF-8）
// 或直接双击 csharp/demo.cmd（csharp\demo.cmd 某文件.txt）。
//
// 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此五个实现
// （C++ / GDScript / JavaScript / C# / Python）的 demo 输出可以互相 diff，
// 见 docs/zh-cn/test-plan.md 的「demo 门禁」。
//
// 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
//
// 本目录是独立的工程（demo/Demo01.csproj），只编译本文件与 ../nevext/Nevext.cs；
// 测试工程 csharp/nevext.csproj 不包含 demo/，两处的 Main 不会冲突。
// ---------------------------------------------------------------------------

using System;
using System.IO;
using System.Text;

namespace NevextDemo
{
    internal static class Demo01
    {
        // 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
        private const string Builtin =
            "  // 单行注释\n" +
            "  hello \"world\";\n" +
            "  /*            \n" +
            "  多行注释      \n" +
            "  */            \n" +
            "  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾";

        /// <summary>无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 null。</summary>
        private static string ReadInput(string[] args)
        {
            if (args.Length < 1)
            {
                return Builtin;
            }
            string path = args[0];
            try
            {
                // StreamReader 会识别并跳过 UTF-8 BOM，与其他实现口径一致
                return File.ReadAllText(path, Encoding.UTF8);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                // 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
                Console.Error.WriteLine("cannot open: " + path);
                return null;
            }
        }

        private static int Main(string[] args)
        {
            // 输出含中文，Windows 控制台默认代码页不是 UTF-8，显式指定
            Console.OutputEncoding = Encoding.UTF8;

            string input = ReadInput(args);
            if (input == null)
            {
                return 2;
            }

            var lexer = new Nevext.Lexer(input);
            var parser = new Nevext.Parser(lexer);
            Nevext.ParseResult res = parser.ParseProgram();

            if (res.Err != Nevext.Result.Success)
            {
                // 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
                Console.WriteLine("==== Lexer errors =============================");
                foreach (string e in lexer.Errors)
                {
                    Console.WriteLine(e);
                }
                Console.WriteLine("==== Parser errors =============================");
                foreach (string e in parser.Errors)
                {
                    Console.WriteLine(e);
                }
                return 1;
            }

            Console.WriteLine("statements num: " + res.Val.Count);
            for (int i = 0; i < res.Val.Count; i++)
            {
                Nevext.Statement stmt = res.Val[i];
                Console.WriteLine("statement " + (i + 1) + " at line " + stmt.Line + ":");
                // 与 C++ 一致：每个串后跟一个空格
                var line = new StringBuilder("  ");
                foreach (string s in stmt.Strings)
                {
                    line.Append(s).Append(' ');
                }
                Console.WriteLine(line.ToString());
            }
            return 0;
        }
    }
}
