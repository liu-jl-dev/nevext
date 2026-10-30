// ---------------------------------------------------------------------------
// test_framework.js —— 极简单元测试框架（零第三方依赖）
//
// 设计取舍与 cpp/test/test_framework.hpp 一致：
//   · 不引入任何测试框架：nevext 保持零依赖，node 一条命令就能跑；
//   · 失败只累加计数并打印，单个断言失败不影响后续用例；
//   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
// ---------------------------------------------------------------------------

class Context {
  constructor(name) {
    this.name = name;
    this.checks = 0;
    this.failures = 0;
  }
}

const registry = [];

function register(name, fn) {
  registry.push([name, fn]);
}

/** 把控制字符转成可见转义，避免诊断信息把终端刷乱。
 *  与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：
 *  六个具名控制字符用 \n \r \t \v \b \f，反斜杠翻倍，其余控制字符 \xNN。 */
function show(s) {
  let out = '';
  for (let i = 0; i < s.length; i += 1) {
    const ch = s[i];
    const unit = s.charCodeAt(i);
    if (ch === '\n') {
      out += '\\n';
    } else if (ch === '\r') {
      out += '\\r';
    } else if (ch === '\t') {
      out += '\\t';
    } else if (ch === '\v') {
      out += '\\v';
    } else if (ch === '\b') {
      out += '\\b';
    } else if (ch === '\f') {
      out += '\\f';
    } else if (ch === '\\') {
      out += '\\\\';
    } else if (unit < 0x20 || unit === 0x7f) {
      out += '\\x' + unit.toString(16).toUpperCase().padStart(2, '0');
    } else {
      out += ch;
    }
  }
  return out;
}

function quote(v) {
  return '"' + show(String(v)) + '"';
}

function reportFailure(ctx, label, detail) {
  ctx.failures += 1;
  console.log('    [FAIL] ' + label + '\n           ' + detail);
}

function checkTrue(ctx, cond, label) {
  ctx.checks += 1;
  if (!cond) {
    reportFailure(ctx, label, '期望为真');
  }
}

function checkFalse(ctx, cond, label) {
  ctx.checks += 1;
  if (cond) {
    reportFailure(ctx, label, '期望为假');
  }
}

function checkEq(ctx, expected, actual, label) {
  ctx.checks += 1;
  const expText = typeof expected === 'string' ? quote(expected) : String(expected);
  const actText = typeof actual === 'string' ? quote(actual) : String(actual);
  if (expected !== actual) {
    reportFailure(ctx, label, '期望 ' + expText + '，实际 ' + actText);
  }
}

/** 全部用例跑一遍；argv 支持 <名字子串> 与 -v。返回退出码。 */
function runAll(argv) {
  let filter = '';
  let verbose = false;
  for (const arg of argv) {
    if (arg === '-v' || arg === '--verbose') {
      verbose = true;
    } else if (arg !== '') {
      filter = arg;
    }
  }

  let caseFailed = 0;
  let casePassed = 0;
  let totalChecks = 0;
  let totalFailures = 0;

  console.log('==== nevext JavaScript 单元测试 ====');
  for (const [name, fn] of registry) {
    if (filter !== '' && !name.includes(filter)) {
      continue;
    }
    const ctx = new Context(name);
    console.log('[ RUN  ] ' + name);
    try {
      fn(ctx);
    } catch (e) {
      ctx.failures += 1;
      console.log('    [FAIL] 用例抛出异常: ' + (e && e.stack ? e.stack : e));
    }
    totalChecks += ctx.checks;
    totalFailures += ctx.failures;
    if (ctx.failures !== 0) {
      caseFailed += 1;
      console.log('[ FAIL ] ' + name + '   失败断言 ' + ctx.failures + '/' + ctx.checks);
    } else {
      casePassed += 1;
      console.log('[  OK  ] ' + name + '   ' + ctx.checks + ' 个断言通过');
    }
  }

  console.log(
    '--------------------------------------------------\n' +
      '用例: ' + (casePassed + caseFailed) + '，通过: ' + casePassed +
      '，失败: ' + caseFailed + '\n' +
      '断言: ' + totalChecks + '，失败: ' + totalFailures
  );
  if (caseFailed !== 0) {
    console.log('结果: 失败');
    return 1;
  }
  console.log('结果: 全部通过');
  return 0;
}

export {
  Context,
  register,
  checkTrue,
  checkFalse,
  checkEq,
  show,
  runAll,
};
