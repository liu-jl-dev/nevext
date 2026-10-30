
// ---------------------------------------------------------------------------
// test_main.js —— 测试入口。
//
// 用法（与 cpp/test、gdscript/test 的入口约定对齐）：
//   node test/test_main.js                  跑全部用例（Conformance + Impl）
//   node test/test_main.js Conformance.C0   只跑名字含该子串的用例
//   node test/test_main.js -v               详细模式（保留参数兼容）
//   node test/test_main.js --dump           按 C++ 侧同一行格式打印 55 条语料
//                                           的实际结果，用于跨实现 diff 门禁
// ---------------------------------------------------------------------------

// 两个套件在被 import 时完成用例注册（Conformance.* 55 条 + Impl.* 5 条）
import { dumpConformance } from './test_conformance.js';
import { runAll } from './test_framework.js';
import './test_impl.js';

const args = process.argv.slice(2);
if (args.includes('--dump')) {
  dumpConformance();
  process.exitCode = 0;
} else {
  process.exitCode = runAll(args);
}
