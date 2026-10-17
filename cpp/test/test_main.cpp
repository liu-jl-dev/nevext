// nevext C++ 单元测试入口
//
// 用法：
//   nevext_test                 运行全部用例
//   nevext_test Lexer           只运行名字含 "Lexer" 的用例
//   nevext_test Parser.Escape   精确到单个用例
//   nevext_test -v              详细输出
//   nevext_test --dump          按统一行格式打印跨实现通用语料的实际结果
//                               （与 gdscript/test/test_main.gd --dump 对照）

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>

#include "test_conformance.hpp"
#include "test_framework.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--dump") {
      nevext_test::DumpConformance();
      return 0;
    }
  }
  return nevext_test::RunAll(argc, argv);
}
