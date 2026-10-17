// nevext C++ 单元测试入口
//
// 用法：
//   nevext_test                 运行全部用例
//   nevext_test Lexer           只运行名字含 "Lexer" 的用例
//   nevext_test Parser.Escape   精确到单个用例
//   nevext_test -v              详细输出

#ifdef _WIN32
#include <windows.h>
#endif

#include "test_framework.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  return nevext_test::RunAll(argc, argv);
}
