-- ---------------------------------------------------------------------------
-- demo01.lua —— Nevext Lua 实现的示例程序。
--
-- 运行：
--   lua lua/demo/demo01.lua              解析内置示例
--   lua lua/demo/demo01.lua 某个文件.txt  解析该文件（UTF-8）
--
-- 与 cpp/demo/demo01.cpp 使用同一段内置输入、同一输出格式，因此六套实现
-- （C++ / GDScript / JavaScript / C# / Python / Lua）的 demo 输出可以互相 diff
-- （diff --strip-trailing-cr），见 docs/zh-cn/test-plan.md 的「demo 门禁」。
--
-- 退出码：0 = 解析成功，1 = 有词法/语法错误，2 = 输入文件打不开。
-- ---------------------------------------------------------------------------

local here = (arg and arg[0] and arg[0]:match("^(.*)[/\\]")) or "."
package.path = here .. "/../nevext/?.lua;" .. package.path

local Nevext = require("nevext")

-- 内置示例：单行注释、裸串与引号串、多行注释、末句省略分号。
local BUILTIN = table.concat({
  "  // 单行注释",
  "  hello \"world\";",
  "  /*            ",
  "  多行注释      ",
  "  */            ",
  "  海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾",
}, "\n")

--- 无参数时用内置示例；传入文件路径时按 UTF-8 读入，失败返回 nil。
local function read_input()
  if not arg or #arg < 1 or arg[1] == nil then
    return BUILTIN
  end
  local path = arg[1]
  local file = io.open(path, "rb")
  if not file then
    -- 与其他实现保持同一句提示文本（走 stderr，不污染 stdout 的解析结果）
    io.stderr:write("cannot open: " .. path .. "\n")
    return nil
  end
  local text = file:read("*a")
  file:close()
  -- 去掉可能存在的 UTF-8 BOM，避免它被当成输入的第一个字节
  if
    string.byte(text, 1) == 0xEF
    and string.byte(text, 2) == 0xBB
    and string.byte(text, 3) == 0xBF
  then
    text = string.sub(text, 4)
  end
  -- 与 Python 文本模式的 universal newlines 口径一致：\r\n 与孤立的 \r 都折算成 \n
  text = text:gsub("\r\n", "\n"):gsub("\r", "\n")
  return text
end

local input = read_input()
if input == nil then
  os.exit(2, true)
end

local lexer = Nevext.Lexer.new(input)
local parser = Nevext.Parser.new(lexer)
local res = parser:parse_program()

if res.err == Nevext.Result.Error then
  -- 词法错误与语法错误分开打印，与 C++ 版的两个错误容器一致。
  io.write("==== Lexer errors =============================\n")
  for _, e in ipairs(lexer.errors) do
    io.write(e, "\n")
  end
  io.write("==== Parser errors =============================\n")
  for _, e in ipairs(parser.errors) do
    io.write(e, "\n")
  end
  os.exit(1, true)
end

io.write("statements num: ", tostring(#res.val), "\n")
for i, stmt in ipairs(res.val) do
  io.write("statement ", tostring(i), " at line ", tostring(stmt.line), ":\n")
  -- 与 C++ 一致：每个串后跟一个空格
  local line = "  "
  for _, s in ipairs(stmt.strings) do
    line = line .. s .. " "
  end
  io.write(line, "\n")
end
os.exit(0, true)
