#include <iostream>
#include <windows.h>
#include "../nevext/nevext.hpp"
#include "utf.hpp"
using namespace std;

int main() {
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  std::u16string input = u"\
  // 单行注释\n\
  hello \"world\";\n\
  /*            \n\
  多行注释      \n\
  */            \n\
  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾";
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
  for (int i = 0; i < prog.size(); i++) {
    cout << "statement " << i + 1 << " at line " << prog[i].line << ":\n  ";
    for (auto& str : prog[i].strings) {
      string s = utf::Utf16ToUtf8(str);
      cout << s << " ";
    }
    cout << std::endl;
  }
  return 0;
}
