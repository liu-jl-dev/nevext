// ---------------------------------------------------------------------------
// demo01.js —— Nevext JavaScript 实现的示例程序。
//
// 运行：
//   node nodejs/demo/demo01.js              解析内置示例
//   node nodejs/demo/demo01.js 某个文件.txt  解析该文件（UTF-8）
//
// 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此五个实现
// （C++ / GDScript / JavaScript / C# / Python）的 demo 输出可以互相 diff，
// 见 docs/zh-cn/test-plan.md 的「demo 门禁」。
//
// 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
// ---------------------------------------------------------------------------

import { readFileSync } from 'node:fs';

import { Lexer, Parser } from '../nevext/nevext.js';

// 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
const BUILTIN = [
  '  // 单行注释',
  '  hello "world";',
  '  /*            ',
  '  多行注释      ',
  '  */            ',
  '  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾',
].join('\n');

/** 无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 null。 */
function readInput() {
  const path = process.argv[2];
  if (path === undefined) return BUILTIN;
  let text;
  try {
    text = readFileSync(path, 'utf8');
  } catch (e) {
    // 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
    console.error('cannot open: ' + path);
    return null;
  }
  // 去掉可能存在的 UTF-8 BOM，避免它被当成输入的第一个字符
  return text.charCodeAt(0) === 0xfeff ? text.slice(1) : text;
}

const input = readInput();
if (input === null) {
  process.exit(2);
}

const lexer = new Lexer(input);
const parser = new Parser(lexer);
const res = parser.parseProgram();

if (res.err) {
  // 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
  console.log('==== Lexer errors =============================');
  for (const e of lexer.errors) console.log(e);
  console.log('==== Parser errors =============================');
  for (const e of parser.errors) console.log(e);
  process.exitCode = 1;
} else {
  console.log('statements num: ' + res.val.length);
  res.val.forEach((stmt, i) => {
    console.log('statement ' + (i + 1) + ' at line ' + stmt.line + ':');
    let line = '  ';
    for (const s of stmt.strings) line += s + ' ';
    console.log(line);
  });
}
