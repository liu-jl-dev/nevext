
// ---------------------------------------------------------------------------
// test_conformance.js —— 跨实现通用语料测试（55 条，A01 ~ H04）
//
// 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
// cpp/test/test_conformance.cpp，GDScript 侧见 gdscript/test/test_conformance.gd。
// 三边必须完全一致：ID、输入、pause 开关、期望的返回值/产物/错误序列。
// 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。
//
// 输入与期望串一律用 JS 转义书写：\n \r \t \v \b \f \\ \" \'，非 ASCII 一律
// 写 \uXXXX 码元（含代理对逐项展开），与文档「避免源文件编码差异」的约定一致。
// ---------------------------------------------------------------------------

import { Lexer, Parser, Result } from '../nevext/nevext.js';
import { checkEq, register, show } from './test_framework.js';

// ------------------------------------------------------------------ 期望值表

const St = (line, strings) => ({ line, strings });
const Er = (line, kind) => ({ line, kind });

// prettier-ignore
const CASES = [
  // ---- A 组 程序骨架与语句切分
  { id: 'A01', src: '',                        ok: true,  pause: true,  prog: [],                                             errs: [] },
  { id: 'A02', src: 'abc',                     ok: true,  pause: true,  prog: [St(1, ['abc'])],                               errs: [] },
  { id: 'A03', src: 'a, b\tc',                 ok: true,  pause: true,  prog: [St(1, ['a', 'b', 'c'])],                       errs: [] },
  { id: 'A04', src: 'a b; c d; e',             ok: true,  pause: true,  prog: [St(1, ['a', 'b']), St(1, ['c', 'd']), St(1, ['e'])], errs: [] },
  { id: 'A05', src: 'a; b',                    ok: true,  pause: true,  prog: [St(1, ['a']), St(1, ['b'])],                   errs: [] },
  { id: 'A06', src: ';;;a;',                   ok: true,  pause: true,  prog: [St(1, ['a'])],                                 errs: [] },
  { id: 'A07', src: 'a,,b',                    ok: true,  pause: true,  prog: [St(1, ['a', 'b'])],                            errs: [] },
  { id: 'A08', src: '  \t\n ',                 ok: true,  pause: true,  prog: [],                                             errs: [] },
  { id: 'A09', src: 'a b\nc d;',               ok: true,  pause: true,  prog: [St(1, ['a', 'b', 'c', 'd'])],                  errs: [] },
  { id: 'A10', src: 'windowSize 1280, 720;',   ok: true,  pause: true,  prog: [St(1, ['windowSize', '1280', '720'])],         errs: [] },

  // ---- B 组 引号串
  { id: 'B01', src: '"hello world" x',         ok: true,  pause: true,  prog: [St(1, ['hello world', 'x'])],                  errs: [] },
  { id: 'B02', src: "'abc'",                   ok: true,  pause: true,  prog: [St(1, ['abc'])],                               errs: [] },
  { id: 'B03', src: '"" z',                    ok: true,  pause: true,  prog: [St(1, ['', 'z'])],                             errs: [] },
  { id: 'B04', src: "'a' \"b\" c",             ok: true,  pause: true,  prog: [St(1, ['a', 'b', 'c'])],                       errs: [] },
  { id: 'B05', src: "'\"' \"'\"",              ok: true,  pause: true,  prog: [St(1, ['"', "'"])],                            errs: [] },
  { id: 'B06', src: '"a"b',                    ok: true,  pause: true,  prog: [St(1, ['a', 'b'])],                            errs: [] },

  // ---- C 组 转义（只在语法层对 QuotedStr 展开）
  { id: 'C01', src: '"a\\nb"',                 ok: true,  pause: true,  prog: [St(1, ['a\nb'])],                              errs: [] },
  { id: 'C02', src: '"\\n\\r\\t\\v\\b\\f"',    ok: true,  pause: true,  prog: [St(1, ['\n\r\t\v\b\f'])],                      errs: [] },
  { id: 'C03', src: '"a\\\\"',                 ok: true,  pause: true,  prog: [St(1, ['a\\'])],                               errs: [] },
  { id: 'C04', src: '"ab\\"cd"',               ok: true,  pause: true,  prog: [St(1, ['ab"cd'])],                             errs: [] },
  { id: 'C05', src: "'a\\qb'",                 ok: false, pause: true,  prog: [St(1, ['a'])],                                 errs: [Er(1, 'InvalidEscape')] },
  { id: 'C06', src: '"a\\qb"; c',              ok: false, pause: true,  prog: [St(1, ['a'])],                                 errs: [Er(1, 'InvalidEscape')] },
  { id: 'C07', src: '"a\\qb"; c',              ok: false, pause: false, prog: [St(1, ['a']), St(1, ['c'])],                   errs: [Er(1, 'InvalidEscape')] },
  // X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
  { id: 'C08', src: '"a\\\nb"',                ok: false, pause: true,  prog: [St(1, ['a'])],                                 errs: [Er(1, 'InvalidEscape')] },
  { id: 'C09', src: '"x\\ny\\tz"',             ok: true,  pause: true,  prog: [St(1, ['x\ny\tz'])],                           errs: [] },

  // ---- D 组 裸串边界（裸串不展开转义）
  { id: 'D01', src: 'a\\b',                    ok: true,  pause: true,  prog: [St(1, ['a\\b'])],                              errs: [] },
  { id: 'D02', src: 'a\\nb',                   ok: true,  pause: true,  prog: [St(1, ['a\\nb'])],                             errs: [] },
  { id: 'D03', src: 'a\\qb',                   ok: true,  pause: true,  prog: [St(1, ['a\\qb'])],                             errs: [] },
  { id: 'D04', src: 'a"b',                     ok: false, pause: true,  prog: [St(1, ['a'])],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  { id: 'D05', src: 'a"b',                     ok: false, pause: false, prog: [St(1, ['a']), St(1, ['b'])],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  { id: 'D06', src: "a'b",                     ok: false, pause: false, prog: [St(1, ['a']), St(1, ['b'])],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  { id: 'D07', src: 'a//comment',              ok: true,  pause: true,  prog: [St(1, ['a'])],                                 errs: [] },
  { id: 'D08', src: 'a/**/ b',                 ok: true,  pause: true,  prog: [St(1, ['a', 'b'])],                            errs: [] },
  { id: 'D09', src: '"a//b"',                  ok: true,  pause: true,  prog: [St(1, ['a//b'])],                              errs: [] },
  // 退格不是裸串终止字符：整串是一个 RawStr
  { id: 'D10', src: 'a\bb',                    ok: true,  pause: false, prog: [St(1, ['a\bb'])],                              errs: [] },

  // ---- E 组 注释（未闭合的多行注释不是错误）
  { id: 'E01', src: '// only comment',         ok: true,  pause: true,  prog: [],                                             errs: [] },
  { id: 'E02', src: '/* c1\nc2 */ d',          ok: true,  pause: true,  prog: [St(2, ['d'])],                                 errs: [] },
  { id: 'E03', src: 'a /* xx',                 ok: true,  pause: true,  prog: [St(1, ['a'])],                                 errs: [] },
  { id: 'E04', src: 'a /* x\nb; c;',           ok: true,  pause: true,  prog: [St(1, ['a'])],                                 errs: [] },
  { id: 'E05', src: '/**/a',                   ok: true,  pause: true,  prog: [St(1, ['a'])],                                 errs: [] },
  { id: 'E06', src: 'a //c1\n/* c2 */ b',      ok: true,  pause: true,  prog: [St(1, ['a', 'b'])],                            errs: [] },

  // ---- F 组 行号
  { id: 'F01', src: 'a;\nb;\nc;',              ok: true,  pause: true,  prog: [St(1, ['a']), St(2, ['b']), St(3, ['c'])],     errs: [] },
  { id: 'F02', src: 'a;\r\nb;',                ok: true,  pause: true,  prog: [St(1, ['a']), St(2, ['b'])],                   errs: [] },
  { id: 'F03', src: '"a\nb"; c;',              ok: true,  pause: true,  prog: [St(1, ['a\nb']), St(2, ['c'])],                errs: [] },
  { id: 'F04', src: '"a\\\nb"; c;',            ok: false, pause: true,  prog: [St(1, ['a'])],                                 errs: [Er(1, 'InvalidEscape')] },
  // 词法错误报在串结束行，语法错误报在串起始行
  { id: 'F05', src: 'a "b\nc',                 ok: false, pause: true,  prog: [St(1, ['a'])],
    errs: [Er(2, 'UnclosedString'), Er(1, 'UnexpectedToken')] },

  // ---- G 组 错误与错误恢复
  { id: 'G01', src: 'a "bc',                   ok: false, pause: true,  prog: [St(1, ['a'])],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  { id: 'G02', src: 'a "bc',                   ok: false, pause: false, prog: [St(1, ['a']), St(1, ['bc'])],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  // 同一行的多处错误都要记录
  { id: 'G03', src: '"a\\qb" "c\\qd"; e',      ok: false, pause: false, prog: [St(1, ['a', 'c']), St(1, ['e'])],
    errs: [Er(1, 'InvalidEscape'), Er(1, 'InvalidEscape')] },
  { id: 'G04', src: '"abc',                    ok: false, pause: true,  prog: [],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  { id: 'G05', src: '"a\nb',                   ok: false, pause: true,  prog: [],
    errs: [Er(2, 'UnclosedString'), Er(1, 'UnexpectedToken')] },

  // ---- H 组 编码与其它
  { id: 'H01', src: 'a;"b";c',                 ok: true,  pause: true,  prog: [St(1, ['a']), St(1, ['b']), St(1, ['c'])],     errs: [] },
  // 非 BMP 字符：C++ 内部为 UTF-16 代理对，JS 同为码元序列，内容须相等
  { id: 'H02', src: '"\uD83D\uDE00" x',        ok: true,  pause: true,  prog: [St(1, ['\uD83D\uDE00', 'x'])],                 errs: [] },
  { id: 'H03', src: '"',                       ok: false, pause: true,  prog: [],
    errs: [Er(1, 'UnclosedString'), Er(1, 'UnexpectedToken')] },
  // 同样为了避免源文件编码差异，非 ASCII 一律写 \uXXXX 码元
  { id: 'H04', src: '\u6D77\u4E0A\u751F\u660E\u6708 \u5929\u6DAF\u5171\u6B64\u65F6',
    ok: true, pause: true, prog: [St(1, ['\u6D77\u4E0A\u751F\u660E\u6708', '\u5929\u6DAF\u5171\u6B64\u65F6'])], errs: [] },
];

// ------------------------------------------------------------------ 驱动逻辑

/** 一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。 */
function run(src, pauseWhenError) {
  const lexer = new Lexer(src);
  const parser = new Parser(lexer);
  const res = parser.parseProgram(pauseWhenError);
  return { result: res.err, prog: res.val, lexErrors: lexer.errors, parseErrors: parser.errors };
}

/** 错误消息 -> (行号, 种类) 语义序列。词法错误在前，语法错误在后。 */
function semErrors(out) {
  const kindOf = (msg) => {
    if (msg.includes('unclosed string literal')) return 'UnclosedString';
    if (msg.includes('invalid escape character')) return 'InvalidEscape';
    if (msg.includes('unexpected token')) return 'UnexpectedToken';
    return 'RAW(' + msg + ')';
  };
  const lineOf = (msg) => {
    const p = msg.indexOf('line ');
    return p < 0 ? -1 : parseInt(msg.slice(p + 5), 10);
  };
  return [...out.lexErrors, ...out.parseErrors].map((e) => lineOf(e) + ':' + kindOf(e));
}

function checkCase(ctx, id) {
  const want = CASES.find((c) => c.id === id);
  if (want === undefined) {
    ctx.failures += 1;
    console.log('    [FAIL] 语料里没有这条用例: ' + id);
    return;
  }

  const out = run(want.src, want.pause);

  // 1) 返回值
  checkEq(ctx, want.ok ? 'Success' : 'Error', out.result === Result.Success ? 'Success' : 'Error', '返回值');

  // 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
  checkEq(ctx, want.prog.length, out.prog.length, '语句数');
  const n = Math.min(out.prog.length, want.prog.length);
  for (let i = 0; i < n; i += 1) {
    checkEq(ctx, want.prog[i].line, out.prog[i].line, '语句[' + i + ']行号');
    checkEq(ctx, want.prog[i].strings.length, out.prog[i].strings.length, '语句[' + i + ']字符串数');
    const m = Math.min(out.prog[i].strings.length, want.prog[i].strings.length);
    for (let j = 0; j < m; j += 1) {
      checkEq(ctx, want.prog[i].strings[j], out.prog[i].strings[j], '语句[' + i + ']字符串[' + j + ']');
    }
  }

  // 3) 错误：按 (行号,种类) 序列比较，顺序敏感
  const got = semErrors(out);
  const exp = want.errs.map((e) => e.line + ':' + e.kind);
  checkEq(ctx, exp.length, got.length, '错误数');
  const k = Math.min(got.length, exp.length);
  for (let i = 0; i < k; i += 1) {
    checkEq(ctx, exp[i], got[i], '错误[' + i + ']');
  }
}

// dump 用：show 已把 \ 转成 \\；在其之后再转义引号才是安全的
function quoteDump(s) {
  return s.split('"').join('\\"');
}

/** 按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。 */
function dumpConformance() {
  for (const c of CASES) {
    const out = run(c.src, c.pause);
    const prog = out.prog
      .map(
        (stmt) =>
          stmt.line +
          ':[' +
          stmt.strings.map((s) => '"' + quoteDump(show(s)) + '"').join(',') +
          ']'
      )
      .join('|');
    const errs = semErrors(out).join(',');
    const ok = out.result === Result.Success ? 'OK' : 'Err';
    console.log(c.id + '|' + ok + '|' + prog + '|' + errs + '|pause=' + (c.pause ? '1' : '0'));
  }
}

// ------------------------------------------------------------------ 用例注册
// 每条语料一个独立用例名，支持 node test_main.js Conformance.C0 这样的前缀筛选。
for (const c of CASES) {
  register('Conformance.' + c.id, (ctx) => checkCase(ctx, c.id));
}

export { CASES, checkCase, dumpConformance, run, semErrors };
