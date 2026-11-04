-- ---------------------------------------------------------------------------
-- test_conformance.lua —— 跨实现通用语料测试（55 条，A01 ~ H04）
--
-- 语料与期望值定义在 docs/zh-cn/test-plan.md 第 3 节；C++ 侧见
-- cpp/test/test_conformance.cpp，GDScript 侧见 gdscript/test/test_conformance.gd，
-- JavaScript 侧见 nodejs/test/test_conformance.js，C# 侧见 csharp/test/TestConformance.cs，
-- Python 侧见 python/test/test_conformance.py。
-- 各实现必须完全一致：ID、输入、pause 开关、期望的返回值/产物/错误序列。
-- 本文件只做「驱动」，不含任何期望值推导；期望值全部来自文档表格。
--
-- 输入与期望串一律用 Lua 转义书写：\n \r \t \v \b \f \\ \" \'，非 ASCII 一律
-- 写 \u{XXXX} 码点（Lua 5.3+ 会生成对应 UTF-8 字节序列），与文档「避免源文件
-- 编码差异」的约定一致。
-- 注意 H02：Lua 的字符串是字节序列，emoji 按 UTF-8 写作 4 个字节；
-- C++ / JS / C# 内部是 UTF-16 代理对，Python 是 1 个码点。三者是同一字符，
-- 通用语料只断言内容相等（见文档 X2 / PY1 / LU1）。
-- ---------------------------------------------------------------------------

local here = (arg and arg[0] and arg[0]:match("^(.*)[/\\]")) or "."
package.path = here .. "/../nevext/?.lua;" .. here .. "/?.lua;" .. package.path

local nevext = require("nevext")
local fw = require("test_framework")

local Lexer = nevext.Lexer
local Parser = nevext.Parser
local Result = nevext.Result
local check_eq = fw.check_eq
local show = fw.show

local M = {}

-- ------------------------------------------------------------------ 期望值表

local function St(line, ...)
  return { line = line, strings = { ... } }
end

local function Er(line, kind)
  return { line = line, kind = kind }
end

local CASES = {
  -- ---- A 组 程序骨架与语句切分
  { id = "A01", src = "", ok = true, pause = true, prog = {}, errs = {} },
  { id = "A02", src = "abc", ok = true, pause = true, prog = { St(1, "abc") }, errs = {} },
  { id = "A03", src = "a, b\tc", ok = true, pause = true, prog = { St(1, "a", "b", "c") }, errs = {} },
  {
    id = "A04",
    src = "a b; c d; e",
    ok = true,
    pause = true,
    prog = { St(1, "a", "b"), St(1, "c", "d"), St(1, "e") },
    errs = {},
  },
  { id = "A05", src = "a; b", ok = true, pause = true, prog = { St(1, "a"), St(1, "b") }, errs = {} },
  { id = "A06", src = ";;;a;", ok = true, pause = true, prog = { St(1, "a") }, errs = {} },
  { id = "A07", src = "a,,b", ok = true, pause = true, prog = { St(1, "a", "b") }, errs = {} },
  { id = "A08", src = "  \t\n ", ok = true, pause = true, prog = {}, errs = {} },
  { id = "A09", src = "a b\nc d;", ok = true, pause = true, prog = { St(1, "a", "b", "c", "d") }, errs = {} },
  {
    id = "A10",
    src = "windowSize 1280, 720;",
    ok = true,
    pause = true,
    prog = { St(1, "windowSize", "1280", "720") },
    errs = {},
  },
  -- ---- B 组 引号串
  { id = "B01", src = "\"hello world\" x", ok = true, pause = true, prog = { St(1, "hello world", "x") }, errs = {} },
  { id = "B02", src = "'abc'", ok = true, pause = true, prog = { St(1, "abc") }, errs = {} },
  { id = "B03", src = "\"\" z", ok = true, pause = true, prog = { St(1, "", "z") }, errs = {} },
  { id = "B04", src = "'a' \"b\" c", ok = true, pause = true, prog = { St(1, "a", "b", "c") }, errs = {} },
  { id = "B05", src = "'\"' \"'\"", ok = true, pause = true, prog = { St(1, "\"", "'") }, errs = {} },
  { id = "B06", src = "\"a\"b", ok = true, pause = true, prog = { St(1, "a", "b") }, errs = {} },
  -- ---- C 组 转义（只在语法层对 QuotedStr 展开）
  { id = "C01", src = "\"a\\nb\"", ok = true, pause = true, prog = { St(1, "a\nb") }, errs = {} },
  { id = "C02", src = "\"\\n\\r\\t\\v\\b\\f\"", ok = true, pause = true, prog = { St(1, "\n\r\t\v\b\f") }, errs = {} },
  { id = "C03", src = "\"a\\\\\"", ok = true, pause = true, prog = { St(1, "a\\") }, errs = {} },
  { id = "C04", src = "\"ab\\\"cd\"", ok = true, pause = true, prog = { St(1, "ab\"cd") }, errs = {} },
  { id = "C05", src = "'a\\qb'", ok = false, pause = true, prog = { St(1, "a") }, errs = { Er(1, "InvalidEscape") } },
  {
    id = "C06",
    src = "\"a\\qb\"; c",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(1, "InvalidEscape") },
  },
  {
    id = "C07",
    src = "\"a\\qb\"; c",
    ok = false,
    pause = false,
    prog = { St(1, "a"), St(1, "c") },
    errs = { Er(1, "InvalidEscape") },
  },
  -- X1：转义表里没有「反斜杠 + 换行」，串内续行写法最终报非法转义
  {
    id = "C08",
    src = "\"a\\\nb\"",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(1, "InvalidEscape") },
  },
  { id = "C09", src = "\"x\\ny\\tz\"", ok = true, pause = true, prog = { St(1, "x\ny\tz") }, errs = {} },
  -- ---- D 组 裸串边界（裸串不展开转义）
  { id = "D01", src = "a\\b", ok = true, pause = true, prog = { St(1, "a\\b") }, errs = {} },
  { id = "D02", src = "a\\nb", ok = true, pause = true, prog = { St(1, "a\\nb") }, errs = {} },
  { id = "D03", src = "a\\qb", ok = true, pause = true, prog = { St(1, "a\\qb") }, errs = {} },
  {
    id = "D04",
    src = "a\"b",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  {
    id = "D05",
    src = "a\"b",
    ok = false,
    pause = false,
    prog = { St(1, "a"), St(1, "b") },
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  {
    id = "D06",
    src = "a'b",
    ok = false,
    pause = false,
    prog = { St(1, "a"), St(1, "b") },
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  { id = "D07", src = "a//comment", ok = true, pause = true, prog = { St(1, "a") }, errs = {} },
  { id = "D08", src = "a/**/ b", ok = true, pause = true, prog = { St(1, "a", "b") }, errs = {} },
  { id = "D09", src = "\"a//b\"", ok = true, pause = true, prog = { St(1, "a//b") }, errs = {} },
  -- 退格不是裸串终止字符：整串是一个 RawStr
  { id = "D10", src = "a\bb", ok = true, pause = false, prog = { St(1, "a\bb") }, errs = {} },
  -- ---- E 组 注释（未闭合的多行注释不是错误）
  { id = "E01", src = "// only comment", ok = true, pause = true, prog = {}, errs = {} },
  { id = "E02", src = "/* c1\nc2 */ d", ok = true, pause = true, prog = { St(2, "d") }, errs = {} },
  { id = "E03", src = "a /* xx", ok = true, pause = true, prog = { St(1, "a") }, errs = {} },
  { id = "E04", src = "a /* x\nb; c;", ok = true, pause = true, prog = { St(1, "a") }, errs = {} },
  { id = "E05", src = "/**/a", ok = true, pause = true, prog = { St(1, "a") }, errs = {} },
  { id = "E06", src = "a //c1\n/* c2 */ b", ok = true, pause = true, prog = { St(1, "a", "b") }, errs = {} },
  -- ---- F 组 行号
  {
    id = "F01",
    src = "a;\nb;\nc;",
    ok = true,
    pause = true,
    prog = { St(1, "a"), St(2, "b"), St(3, "c") },
    errs = {},
  },
  {
    id = "F02",
    src = "a;\r\nb;",
    ok = true,
    pause = true,
    prog = { St(1, "a"), St(2, "b") },
    errs = {},
  },
  { id = "F03", src = "\"a\nb\"; c;", ok = true, pause = true, prog = { St(1, "a\nb"), St(2, "c") }, errs = {} },
  {
    id = "F04",
    src = "\"a\\\nb\"; c;",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(1, "InvalidEscape") },
  },
  -- 词法错误报在串结束行，语法错误报在串起始行
  {
    id = "F05",
    src = "a \"b\nc",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(2, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  -- ---- G 组 错误与错误恢复
  {
    id = "G01",
    src = "a \"bc",
    ok = false,
    pause = true,
    prog = { St(1, "a") },
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  {
    id = "G02",
    src = "a \"bc",
    ok = false,
    pause = false,
    prog = { St(1, "a"), St(1, "bc") },
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  -- 同一行的多处错误都要记录
  {
    id = "G03",
    src = "\"a\\qb\" \"c\\qd\"; e",
    ok = false,
    pause = false,
    prog = { St(1, "a", "c"), St(1, "e") },
    errs = { Er(1, "InvalidEscape"), Er(1, "InvalidEscape") },
  },
  {
    id = "G04",
    src = "\"abc",
    ok = false,
    pause = true,
    prog = {},
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  {
    id = "G05",
    src = "\"a\nb",
    ok = false,
    pause = true,
    prog = {},
    errs = { Er(2, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  -- ---- H 组 编码与其它
  { id = "H01", src = "a;\"b\";c", ok = true, pause = true, prog = { St(1, "a"), St(1, "b"), St(1, "c") }, errs = {} },
  -- 非 BMP 字符：Lua 按 UTF-8 写作 4 个字节；C++ / JS / C# 内部是 UTF-16 代理对
  {
    id = "H02",
    src = "\"\u{1F600}\" x",
    ok = true,
    pause = true,
    prog = { St(1, "\u{1F600}", "x") },
    errs = {},
  },
  {
    id = "H03",
    src = "\"",
    ok = false,
    pause = true,
    prog = {},
    errs = { Er(1, "UnclosedString"), Er(1, "UnexpectedToken") },
  },
  -- 同样为了避免源文件编码差异，非 ASCII 一律写 \u{XXXX} 码点
  {
    id = "H04",
    src = "\u{6D77}\u{4E0A}\u{751F}\u{660E}\u{6708} \u{5929}\u{6DAF}\u{5171}\u{6B64}\u{65F6}",
    ok = true,
    pause = true,
    prog = { St(1, "\u{6D77}\u{4E0A}\u{751F}\u{660E}\u{6708}", "\u{5929}\u{6DAF}\u{5171}\u{6B64}\u{65F6}") },
    errs = {},
  },
}
M.CASES = CASES

-- ------------------------------------------------------------------ 驱动逻辑

--- 一次完整解析（Lexer + Parser），汇总返回值、产物与两侧错误列表。
local function run(src, pause_when_error)
  local lexer = Lexer.new(src)
  local parser = Parser.new(lexer)
  local res = parser:parse_program(pause_when_error)
  return {
    result = res.err,
    prog = res.val,
    lex_errors = lexer.errors,
    parse_errors = parser.errors,
  }
end
M.run = run

--- 错误消息 -> (行号, 种类) 语义序列。词法错误在前，语法错误在后。
local function kind_of(msg)
  if string.find(msg, "unclosed string literal", 1, true) then
    return "UnclosedString"
  end
  if string.find(msg, "invalid escape character", 1, true) then
    return "InvalidEscape"
  end
  if string.find(msg, "unexpected token", 1, true) then
    return "UnexpectedToken"
  end
  return "RAW(" .. msg .. ")"
end

local function line_of(msg)
  local pos = string.find(msg, "line ", 1, true)
  if not pos then
    return -1
  end
  local digits = string.match(msg, "line (%d+)", pos)
  return digits and tonumber(digits) or -1
end

local function sem_errors(out)
  local seq = {}
  for _, e in ipairs(out.lex_errors) do
    seq[#seq + 1] = tostring(line_of(e)) .. ":" .. kind_of(e)
  end
  for _, e in ipairs(out.parse_errors) do
    seq[#seq + 1] = tostring(line_of(e)) .. ":" .. kind_of(e)
  end
  return seq
end
M.sem_errors = sem_errors

local function find_case(case_id)
  for _, c in ipairs(CASES) do
    if c.id == case_id then
      return c
    end
  end
  return nil
end

local function check_case(ctx, case_id)
  local want = find_case(case_id)
  if not want then
    ctx.failures = ctx.failures + 1
    io.write("    [FAIL] 语料里没有这条用例: ", case_id, "\n")
    return
  end

  local out = run(want.src, want.pause)

  -- 1) 返回值
  check_eq(
    ctx,
    want.ok and "Success" or "Error",
    out.result == Result.Success and "Success" or "Error",
    "返回值"
  )

  -- 2) 产物：语句数 → 每条语句的行号 → 每条语句的字符串列表
  check_eq(ctx, #want.prog, #out.prog, "语句数")
  for i = 1, math.min(#out.prog, #want.prog) do
    check_eq(ctx, want.prog[i].line, out.prog[i].line, "语句[" .. i .. "]行号")
    check_eq(ctx, #want.prog[i].strings, #out.prog[i].strings, "语句[" .. i .. "]字符串数")
    for j = 1, math.min(#out.prog[i].strings, #want.prog[i].strings) do
      check_eq(ctx, want.prog[i].strings[j], out.prog[i].strings[j], "语句[" .. i .. "]字符串[" .. j .. "]")
    end
  end

  -- 3) 错误：按 (行号,种类) 序列比较，顺序敏感
  local got = sem_errors(out)
  local exp = {}
  for _, e in ipairs(want.errs) do
    exp[#exp + 1] = tostring(e.line) .. ":" .. e.kind
  end
  check_eq(ctx, #exp, #got, "错误数")
  for i = 1, math.min(#got, #exp) do
    check_eq(ctx, exp[i], got[i], "错误[" .. i .. "]")
  end
end

--- dump 用：show 已把 \ 转成 \\；在其之后再转义引号才是安全的。
local function quote_dump(s)
  return (string.gsub(s, "\"", "\\\""))
end

--- 按 C++ 侧 DumpConformance 的行格式打印 55 条语料的实际结果。
function M.dump_conformance()
  for _, c in ipairs(CASES) do
    local out = run(c.src, c.pause)
    local parts = {}
    for _, stmt in ipairs(out.prog) do
      local quoted = {}
      for _, s in ipairs(stmt.strings) do
        quoted[#quoted + 1] = "\"" .. quote_dump(show(s)) .. "\""
      end
      parts[#parts + 1] = tostring(stmt.line) .. ":[" .. table.concat(quoted, ",") .. "]"
    end
    local prog = table.concat(parts, "|")
    local errs = table.concat(sem_errors(out), ",")
    local ok = out.result == Result.Success and "OK" or "Err"
    io.write(c.id, "|", ok, "|", prog, "|", errs, "|pause=", c.pause and "1" or "0", "\n")
  end
end

-- ------------------------------------------------------------------ 用例注册
-- 每条语料一个独立用例名，支持 test_main.lua Conformance.C0 这样的前缀筛选。

function M.register_all()
  for _, c in ipairs(CASES) do
    fw.register("Conformance." .. c.id, function(ctx)
      check_case(ctx, c.id)
    end)
  end
end

return M
