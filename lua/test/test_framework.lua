-- ---------------------------------------------------------------------------
-- test_framework.lua —— 极简单元测试框架（零第三方依赖）
--
-- 设计取舍与 cpp/test/test_framework.hpp 一致：
--   · 不引入任何测试框架（不用 busted / lunit）：nevext 保持零依赖，
--     lua 一条命令就能跑；
--   · 失败只累加计数并打印，单个断言失败不影响后续用例；
--   · 用例内部用 pcall 兜底，异常计入失败而不是中断整轮；
--   · 支持按用例名子串筛选、-v 详细模式、失败即非零退出码。
-- ---------------------------------------------------------------------------

local M = {}

-- ------------------------------------------------------------ show 转义可视化

-- 与 cpp/test/test_framework.hpp 的 EscapeVisible 逐项一致：六个具名控制字节
-- 用 \n \r \t \v \b \f，反斜杠翻倍，其余控制字节 \xNN（大写十六进制）。
-- Lua 按字节遍历：UTF-8 多字节字符的每个字节都 >= 0x80，走「原样保留」分支，
-- 因此输出与按码点处理的 Python 版逐字节相同。
local SHOW_MAP = {
  [0x0A] = "\\n",
  [0x0D] = "\\r",
  [0x09] = "\\t",
  [0x0B] = "\\v",
  [0x08] = "\\b",
  [0x0C] = "\\f",
  [0x5C] = "\\\\",
}

function M.show(s)
  local out = {}
  for i = 1, #s do
    local b = string.byte(s, i)
    local mapped = SHOW_MAP[b]
    if mapped then
      out[#out + 1] = mapped
    elseif b < 0x20 or b == 0x7F then
      out[#out + 1] = string.format("\\x%02X", b)
    else
      out[#out + 1] = string.sub(s, i, i)
    end
  end
  return table.concat(out)
end

function M.quote(v)
  return '"' .. M.show(tostring(v)) .. '"'
end

-- ------------------------------------------------------------------ 注册表

local registry = {}

--- 注册一个用例。fn 接收 ctx（{name, checks, failures}）。
function M.register(name, fn)
  registry[#registry + 1] = { name = name, fn = fn }
end

--- 清空注册表（只给需要重复注册的极端场景用，正常流程不需要）。
function M.reset()
  registry = {}
end

-- ------------------------------------------------------------------ 断言

local function report_failure(ctx, label, detail)
  ctx.failures = ctx.failures + 1
  io.write("    [FAIL] ", label, "\n           ", detail, "\n")
end

function M.check_true(ctx, cond, label)
  ctx.checks = ctx.checks + 1
  if not cond then
    report_failure(ctx, label, "期望为真")
  end
end

function M.check_false(ctx, cond, label)
  ctx.checks = ctx.checks + 1
  if cond then
    report_failure(ctx, label, "期望为假")
  end
end

function M.check_eq(ctx, expected, actual, label)
  ctx.checks = ctx.checks + 1
  if expected ~= actual then
    local function text(v)
      if type(v) == "string" then
        return M.quote(v)
      end
      return tostring(v)
    end
    report_failure(ctx, label, "期望 " .. text(expected) .. "，实际 " .. text(actual))
  end
end

-- ------------------------------------------------------------------ 执行器

--- 全部用例跑一遍；argv 支持 <名字子串> 与 -v。返回退出码。
function M.run_all(argv)
  local filter_text = ""
  for _, arg_value in ipairs(argv or {}) do
    if arg_value == "-v" or arg_value == "--verbose" then
      -- 详细模式占位：本框架已逐用例打印，保留参数只为兼容
    elseif arg_value ~= "" then
      filter_text = arg_value
    end
  end

  local case_failed, case_passed = 0, 0
  local total_checks, total_failures = 0, 0

  io.write("==== nevext Lua 单元测试 ====\n")
  for _, entry in ipairs(registry) do
    if filter_text == "" or string.find(entry.name, filter_text, 1, true) then
      local ctx = { name = entry.name, checks = 0, failures = 0 }
      io.write("[ RUN  ] ", entry.name, "\n")
      local ok, err = pcall(entry.fn, ctx)
      if not ok then
        -- 用例异常要计入失败而不是中断整轮
        ctx.failures = ctx.failures + 1
        io.write("    [FAIL] 用例抛出异常: ", tostring(err), "\n")
      end
      total_checks = total_checks + ctx.checks
      total_failures = total_failures + ctx.failures
      if ctx.failures ~= 0 then
        case_failed = case_failed + 1
        io.write(
          "[ FAIL ] ",
          entry.name,
          "   失败断言 ",
          tostring(ctx.failures),
          "/",
          tostring(ctx.checks),
          "\n"
        )
      else
        case_passed = case_passed + 1
        io.write("[  OK  ] ", entry.name, "   ", tostring(ctx.checks), " 个断言通过\n")
      end
    end
  end

  io.write(
    "--------------------------------------------------\n",
    "用例: ",
    tostring(case_passed + case_failed),
    "，通过: ",
    tostring(case_passed),
    "，失败: ",
    tostring(case_failed),
    "\n",
    "断言: ",
    tostring(total_checks),
    "，失败: ",
    tostring(total_failures),
    "\n"
  )
  if case_failed ~= 0 then
    io.write("结果: 失败\n")
    return 1
  end
  io.write("结果: 全部通过\n")
  return 0
end

-- ------------------------------------------------------------------ 自检

--- 框架自检：确保 show 的转义与 C++ 侧一致（被 test_main 调起前手工跑一次）。
--- 返回失败数；直接运行本文件（lua lua/test/test_framework.lua）即进入自检模式。
function M.self_check()
  local failures = 0
  local function expect(got, want, label)
    if got ~= want then
      failures = failures + 1
      io.write("[SELF] ", label, ": 期望 ", M.quote(want), "，实际 ", M.quote(got), "\n")
    end
  end
  expect(M.show("\n\r\t\v\b\f"), "\\n\\r\\t\\v\\b\\f", "六个具名控制字符")
  expect(M.show("\\"), "\\\\", "反斜杠翻倍")
  expect(M.show("\x01"), "\\x01", "低控制字节")
  expect(M.show("\x7f"), "\\x7F", "DEL 大写十六进制")
  expect(M.show("\u{6D77}"), "\u{6D77}", "多字节 UTF-8 原样保留")
  return failures
end

if arg and arg[0] and string.find(arg[0], "test_framework%.lua$") then
  os.exit(M.self_check())
end

return M
