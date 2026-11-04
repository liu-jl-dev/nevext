-- ---------------------------------------------------------------------------
-- test_impl.lua —— Lua 实现私有的用例（与 Python 侧 test_impl.py 对应）
--
-- 通用语料（test_conformance.lua）各实现共用；本文件放 Lua 自己关心的
-- 工程性用例：对象复用、终止性、大输入，以及一条字节模型用例。
-- ---------------------------------------------------------------------------

local here = (arg and arg[0] and arg[0]:match("^(.*)[/\\]")) or "."
package.path = here .. "/../nevext/?.lua;" .. here .. "/?.lua;" .. package.path

local nevext = require("nevext")
local fw = require("test_framework")

local Lexer = nevext.Lexer
local Parser = nevext.Parser
local Result = nevext.Result
local TokenType = nevext.TokenType
local check_eq = fw.check_eq
local check_true = fw.check_true

local M = {}

-- LU-01 Lexer.reset / Parser.reset 能让实例重用到另一份源码
--       （与 Python 的 PY-01、GDScript 的 IMP-02、C++ 的 LEX-26 / PAR-18、JS 的 JS-01 对应）
local function reset_are_reusable(ctx)
  local lexer = Lexer.new("a b")
  local parser = Parser.new(lexer)
  local first = parser:parse_program(true)
  check_eq(ctx, 1, #first.val, "第一次语句数")
  check_eq(ctx, "Success", first.err == Result.Success and "Success" or "Error", "第一次返回值")

  lexer:reset("c; d")
  parser:reset(lexer)
  local second = parser:parse_program(true)
  check_eq(ctx, 2, #second.val, "第二次语句数")
  if #second.val >= 2 then
    check_eq(ctx, "c", second.val[1].strings[1], "第 1 条语句")
    check_eq(ctx, "d", second.val[2].strings[1], "第 2 条语句")
  end
  check_eq(ctx, 0, #parser.errors, "Parser 错误数（reset 应清空）")
  check_eq(ctx, 0, #lexer.errors, "Lexer 错误数（reset 应清空）")
  check_eq(ctx, 1, lexer.line, "行号应回到 1")
end

-- LU-02 终止性：病态串的每个字节前缀都要能在阈值内扫到 EOF，不死循环、不爆栈
local function pathological_inputs_terminate(ctx)
  local src = "a \"b\\c' /* xx //\ny ;\t'z\"*/ ;"
  for i = 0, #src do
    local lexer = Lexer.new(string.sub(src, 1, i))
    local reached_eof = false
    for _ = 1, 200 do
      if lexer:next_token().type == TokenType.EndOfFile then
        reached_eof = true
        break
      end
    end
    check_true(ctx, reached_eof, "前缀[" .. i .. "]能在阈值内扫到 EOF")
  end
end

-- LU-03 长裸串不被截断
local function long_raw_token_is_complete(ctx)
  local src = string.rep("x", 1000) .. ";"
  local lexer = Lexer.new(src)
  local token = lexer:next_token()
  check_eq(ctx, "RawStr", token.type == TokenType.RawStr and "RawStr" or "其它", "token 类型")
  check_eq(ctx, 1000, #token.literal, "裸串长度")
end

-- LU-04 大量语句全部解析（游标实现不应有字节级性能悬崖）
local function many_statements_are_all_parsed(ctx)
  local parts = {}
  for i = 0, 499 do
    parts[#parts + 1] = "s" .. i .. " a b;"
  end
  local src = table.concat(parts, "\n")
  local lexer = Lexer.new(src)
  local parser = Parser.new(lexer)
  local res = parser:parse_program(true)
  check_eq(ctx, 500, #res.val, "语句数")
  check_eq(ctx, "Success", res.err == Result.Success and "Success" or "Error", "返回值")
  check_eq(ctx, "s0", res.val[1].strings[1], "首语句首串")
  check_eq(ctx, "s499", res.val[500].strings[1], "末语句首串")
  check_eq(ctx, 500, res.val[500].line, "末语句行号")
end

-- LU-05 非 UTF-8 的任意字节原样保留（Lua 的字符串按字节处理，
--       对应 Python 的 PY-05 孤立代理项用例；另一套实现按各自码元保留，语义相同）
local function arbitrary_bytes_preserved(ctx)
  local lexer = Lexer.new("x \255 y \"\254\"")
  local parser = Parser.new(lexer)
  local res = parser:parse_program(true)
  check_eq(ctx, 1, #res.val, "语句数")
  if #res.val >= 1 then
    local strings = res.val[1].strings
    check_eq(ctx, 4, #strings, "字符串数")
    if #strings >= 4 then
      check_eq(ctx, "\255", strings[2], "裸串中的高位字节原样保留")
      check_eq(ctx, "\254", strings[4], "引号串中的高位字节经语法层原样保留")
    end
  end
  check_eq(ctx, 0, #lexer.errors + #parser.errors, "错误数")
end

function M.register_all()
  fw.register("Impl.ResetAreReusable", reset_are_reusable)
  fw.register("Impl.PathologicalInputsTerminate", pathological_inputs_terminate)
  fw.register("Impl.LongRawTokenIsComplete", long_raw_token_is_complete)
  fw.register("Impl.ManyStatementsAreAllParsed", many_statements_are_all_parsed)
  fw.register("Impl.ArbitraryBytesPreserved", arbitrary_bytes_preserved)
end

return M
