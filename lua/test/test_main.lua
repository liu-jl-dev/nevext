-- ---------------------------------------------------------------------------
-- test_main.lua —— 测试入口。
--
-- 用法（与 cpp/test、gdscript/test、nodejs/test、csharp/test、python/test 的
-- 入口约定对齐）：
--   lua lua/test/test_main.lua           跑全部用例（Conformance + Impl）
--   ... Conformance.C0                   只跑名字含该子串的用例
--   ... -v                               详细模式（保留参数兼容）
--   ... --dump                           按 C++ 侧同一行格式打印 55 条语料的
--                                        实际结果，用于跨实现 diff 门禁
-- ---------------------------------------------------------------------------

local here = (arg and arg[0] and arg[0]:match("^(.*)[/\\]")) or "."
package.path = here .. "/?.lua;" .. here .. "/../nevext/?.lua;" .. package.path

local test_conformance = require("test_conformance")
local test_impl = require("test_impl")
local fw = require("test_framework")

local function main(argv)
  test_conformance.register_all()
  test_impl.register_all()

  for _, a in ipairs(argv or {}) do
    if a == "--dump" then
      test_conformance.dump_conformance()
      return 0
    end
  end
  return fw.run_all(argv)
end

os.exit(main({ table.unpack(arg, 1, arg and #arg or 0) }), true)
