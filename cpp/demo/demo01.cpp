#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <windows.h>
#include "../nevext/nevext.hpp"
#include "utf.hpp"
using namespace std;

// 演示用的内置输入：单行注释、裸串与引号串、多行注释、末句省略分号。
static const char16_t* kBuiltinInput =
    u"\
  // 单行注释\n\
  hello \"world\";\n\
  /*            \n\
  多行注释      \n\
  */            \n\
  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾";

// 无参数时用内置输入；传入文件路径时按 UTF-8 读入该文件。
static bool ReadInput(int argc, char** argv, std::u16string& out) {
  if (argc < 2) {
    out = kBuiltinInput;
    return true;
  }
  std::ifstream ifs(argv[1], std::ios::binary);
  if (!ifs) {
    std::cerr << "cannot open: " << argv[1] << endl;
    return false;
  }
  std::string text((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());
  out = utf::Utf8ToUtf16(text);
  return true;
}

int main(int argc, char** argv) {
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  std::u16string input;
  if (!ReadInput(argc, argv, input)) {
    return 2;
  }

  Nevext::Lexer lexer(input);
  Nevext::Parser parser(lexer);
  Nevext::Program prog;
  auto result = parser.ParseProgram(prog);

  if (result) {
    cout << "==== Lexer errors =============================\n";
    for (auto& err : lexer.errors) {
      cout << err << std::endl;
    }
    cout << "==== Parser errors =============================\n";
    for (auto& err : parser.errors) {
      cout << err << std::endl;
    }
    return 1;
  }

  cout << "statements num: " << prog.size() << std::endl;
  for (std::size_t i = 0; i < prog.size(); i++) {
    cout << "statement " << i + 1 << " at line " << prog[i].line << ":\n  ";
    for (auto& str : prog[i].strings) {
      string s = utf::Utf16ToUtf8(str);
      cout << s << " ";
    }
    cout << std::endl;
  }
  return 0;
}
