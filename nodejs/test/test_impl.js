
// ---------------------------------------------------------------------------
// test_impl.js —— JS 实现私有的用例（与 GDScript 侧 test_impl.gd 对应）
//
// 通用语料（test_conformance.js）三边共用；本文件放 JS 自己关心的工程性
// 用例：对象复用、终止性、大输入，以及一条 JS 特有的码元保真用例。
// ---------------------------------------------------------------------------

import { Lexer, Parser, Result, TokenType } from '../nevext/nevext.js';
import { checkEq, checkTrue, register } from './test_framework.js';

// JS-01 Lexer.reset / Parser.reset 能让实例重用到另一份源码
//       （与 GDScript 的 IMP-02、C++ 的 LEX-26 / PAR-18 对应）
register('Impl.ResetAreReusable', (ctx) => {
  const lexer = new Lexer('a b');
  const parser = new Parser(lexer);
  const first = parser.parseProgram(true);
  checkEq(ctx, 1, first.val.length, '第一次语句数');
  checkEq(ctx, 'Success', first.err === Result.Success ? 'Success' : 'Error', '第一次返回值');

  lexer.reset('c; d');
  parser.reset(lexer);
  const second = parser.parseProgram(true);
  checkEq(ctx, 2, second.val.length, '第二次语句数');
  if (second.val.length >= 2) {
    checkEq(ctx, 'c', second.val[0].strings[0], '第 1 条语句');
    checkEq(ctx, 'd', second.val[1].strings[0], '第 2 条语句');
  }
  checkEq(ctx, 1, lexer.line, '词法器起始行号');
  checkEq(ctx, 0, parser.errors.length, 'Parser 错误数（Reset 应清空）');
  checkEq(ctx, 0, lexer.errors.length, 'Lexer 错误数（Reset 应清空）');
});

// JS-02 终止性：病态串的每个前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
register('Impl.PathologicalInputsTerminate', (ctx) => {
  const src = 'a "b\\c\' /* xx //\ny ;\t\'z"*/ ;';
  for (let i = 0; i <= src.length; i += 1) {
    const lexer = new Lexer(src.slice(0, i));
    let reachedEof = false;
    for (let step = 0; step < 200; step += 1) {
      if (lexer.nextToken().type === TokenType.EndOfFile) {
        reachedEof = true;
        break;
      }
    }
    checkTrue(ctx, reachedEof, '前缀[' + i + ']能在阈值内扫到 EOF');
  }
});

// JS-03 长裸串不被截断
register('Impl.LongRawTokenIsComplete', (ctx) => {
  const src = 'x'.repeat(1000) + ';';
  const lexer = new Lexer(src);
  const token = lexer.nextToken();
  checkEq(ctx, 'RawStr', token.type === TokenType.RawStr ? 'RawStr' : '其它', 'token 类型');
  checkEq(ctx, 1000, token.literal.length, '裸串长度');
});

// JS-04 大量语句全部解析（游标实现不应有码元级性能悬崖）
register('Impl.ManyStatementsAreAllParsed', (ctx) => {
  const lines = [];
  for (let i = 0; i < 500; i += 1) {
    lines.push('s' + i + ' a b;');
  }
  const src = lines.join('\n');
  const lexer = new Lexer(src);
  const parser = new Parser(lexer);
  const res = parser.parseProgram(true);
  checkEq(ctx, 500, res.val.length, '语句数');
  checkEq(ctx, 'Success', res.err === Result.Success ? 'Success' : 'Error', '返回值');
  checkEq(ctx, 's0', res.val[0].strings[0], '首语句首串');
  checkEq(ctx, 's499', res.val[499].strings[0], '末语句首串');
  checkEq(ctx, 500, res.val[499].line, '末语句行号');
});

// JS-05 孤立代理项按码元原样保留（C++ 同样保留；GDScript 的 UTF-32 无法表达）
register('Impl.LoneSurrogatePreserved', (ctx) => {
  const lexer = new Lexer('x \uD800 y "\uDFFF"');
  const parser = new Parser(lexer);
  const res = parser.parseProgram(true);
  checkEq(ctx, 1, res.val.length, '语句数');
  if (res.val.length >= 1) {
    const strings = res.val[0].strings;
    checkEq(ctx, 4, strings.length, '字符串数');
    if (strings.length >= 4) {
      checkEq(ctx, '\uD800', strings[1], '孤立高代理项原样保留');
      checkEq(ctx, '\uDFFF', strings[3], '孤立低代理项经语法层原样保留');
    }
  }
  checkEq(ctx, 0, lexer.errors.length + parser.errors.length, '错误数');
});
