-- ---------------------------------------------------------------------------
-- Nevext 的 Lua 实现（Lua 5.4+，单文件，零依赖）。
--
-- 行为基准：cpp/nevext 下的 C++ 参考实现，55 条跨实现通用语料见
-- docs/zh-cn/test-plan.md；本文件与其保持逐条一致：
--   · TokenType 五个枚举项的取值顺序与 C++ 相同；
--   · 词法错误进 lexer.errors，语法/转义错误进 parser.errors，消息文本逐字一致
--     （含句号后的空格，一致性断言按 (行号, 错误种类) 比较）；
--   · 未闭合的多行注释不是错误，一直延伸到输入结束；
--   · 引号串可以跨行，串内换行（含被反斜杠转义的换行）计入物理行号；
--   · 转义只在语法层对 QuotedStr 展开，失败时保留失败点之前已展开的部分。
--
-- Lua 的 string 是字节序列（C string），与 Python 的码点、C++ 的 UTF-16 码元
-- 都不同；但本语法的全部结构字符（空白、; " ' \ /、\n）都在 ASCII 区间，
-- UTF-8 的多字节字符按编码规则不可能包含这些字节，因此按字节扫描与按码元
-- 扫描行为完全等价，转义展开也只涉及单字节。多字节内容原样保留，
-- 不做任何编解码。预计登记缺口 LU1：字符串元素数按字节计（见文档 X2 / PY1）。
--
-- 与 C++ 的 API 对应关系（Lua 惯例 snake_case，冒号调用传 self）：
--   Lexer::NextToken()            -> lexer:next_token()
--   Lexer::Reset(input)           -> lexer:reset(code)
--   Parser::ParseProgram(prog, p) -> parser:parse_program(pause_when_error)
--                                    （返回 {err, val}，语句列表走返回值）
--   Parser::Reset(lexer)          -> parser:reset(lexer)
--
-- 实现说明：与 JS / C# / Python 相同，「完整字符串 + 游标 pos」，整体 O(n)；
-- 取字符用 string.byte / string.sub，不做逐字节缓冲区前移。
-- ---------------------------------------------------------------------------

local Nevext = {
  -- 库版本。与 CHANGELOG.md、git tag（v 前缀）同步更新；落点一致性见 tools/check_version.py。
  VERSION = "0.1.0",
}

-- 错误消息常量，与 C++ 的消息表逐字一致（含句号后的空格）
Nevext.UNCLOSED_MESSAGE = "unclosed string literal. "
Nevext.INVALID_ESCAPE_MESSAGE = "invalid escape character. "
Nevext.UNEXPECTED_MESSAGE = "unexpected token. "

-- 词法单元类型。取值与 C++ 的 TokenType 枚举逐项对应（0 起）。
Nevext.TokenType = {
  Unexpected = 0, -- 非法 token（未闭合引号串、无法推进的字符等）
  EndOfFile = 1,  -- 输入结束
  RawStr = 2,     -- 未加引号的裸串（不展开转义）
  QuotedStr = 3,  -- 引号串（literal 保留转义序列原文，语法层再展开）
  Sep = 4,        -- 分号
}

-- 解析结果码。Success 为 0，可直接写 if res.err ~= 0。
Nevext.Result = {
  Success = 0,
  Error = 1,
}

local TokenType = Nevext.TokenType
local Result = Nevext.Result

-- 结构字节的 ASCII 码位（UTF-8 多字节字符不可能出现这些字节，查表安全）
local BYTE_TAB, BYTE_NL, BYTE_VT, BYTE_FF, BYTE_CR = 0x09, 0x0A, 0x0B, 0x0C, 0x0D
local BYTE_SPACE, BYTE_COMMA = 0x20, 0x2C
local BYTE_DQUOTE, BYTE_SQUOTE, BYTE_BACKSLASH = 0x22, 0x27, 0x5C
local BYTE_SEMI, BYTE_SLASH = 0x3B, 0x2F
local BYTE_ASTERISK = 0x2A

-- 空白字符集，必须与 C++ 的 whiteChars 逐项一致
local WHITE_BYTES = {
  [BYTE_SPACE] = true, [BYTE_COMMA] = true, [BYTE_TAB] = true,
  [BYTE_VT] = true, [BYTE_FF] = true, [BYTE_NL] = true, [BYTE_CR] = true,
}
-- 裸串终止字符集，必须与 C++ 的 rawStrEndChars 逐项一致
local RAW_END_BYTES = { [BYTE_SEMI] = true, [BYTE_DQUOTE] = true, [BYTE_SQUOTE] = true }

-- 转义表，与 C++ 的 Parser::escapeMap 逐项一致（没有「反斜杠 + 换行」续行）
local ESCAPE_MAP = {
  ["\\"] = "\\",
  ["'"] = "'",
  ['"'] = '"',
  ["n"] = "\n",
  ["r"] = "\r",
  ["t"] = "\t",
  ["v"] = "\v",
  ["b"] = "\b",
  ["f"] = "\f",
}

-- ------------------------------------------------------------------ 词法分析

local Lexer = {}
Lexer.__index = Lexer
Nevext.Lexer = Lexer

--- 新建词法分析器。code 为字节序列；调用方须保证其存活到解析结束。
function Lexer.new(code)
  local self = setmetatable({}, Lexer)
  self:reset(code or "")
  return self
end

--- 复用同一对象解析新输入：行号、游标与错误容器全部回到初始状态。
function Lexer:reset(code)
  self.code = code or ""
  self.pos = 1 -- 游标：1 起的下标，code 中尚未消费部分的起点
  self.line = 1
  self.errors = {}
end

--- 取出下一个 token。输入耗尽后重复调用会一直得到 EndOfFile。
--- 返回 {type, line, literal}；literal 为码元原文，EOF / Unexpected 时为空串。
function Lexer:next_token()
  local token = { type = TokenType.Unexpected, line = 0, literal = "" }
  self:_skip_ws()
  token.line = self.line

  local b = self.code:byte(self.pos)
  if not b then
    token.type = TokenType.EndOfFile
  elseif b == BYTE_SEMI then
    token.type = TokenType.Sep
    token.literal = ";"
    self.pos = self.pos + 1
  elseif b == BYTE_DQUOTE or b == BYTE_SQUOTE then
    local err, val = self:_scan_quoted_string(b)
    if err ~= Result.Success then
      -- 出错只吞掉开头引号，后续 token 仍可继续读出（与 C++ 一致）
      token.type = TokenType.Unexpected
    else
      token.type = TokenType.QuotedStr
      token.literal = val
    end
  else
    local err, val = self:_scan_raw_string()
    if err ~= Result.Success then
      -- 一个字节都无法推进（撞上终止字符），吞掉 1 个字节避免死循环
      token.type = TokenType.Unexpected
      self.pos = self.pos + 1
    else
      token.type = TokenType.RawStr
      token.literal = val
    end
  end
  return token
end

--- 引号串扫描：保留转义序列原文，跨行与被转义的换行都推进物理行号。
--- 成功返回 (Result.Success, 内容)；失败返回 (Result.Error, "")。
function Lexer:_scan_quoted_string(quote_byte)
  self.pos = self.pos + 1 -- 跳过开头的引号
  local start = self.pos
  local length = 0
  while true do
    if self.pos + length > #self.code then
      self:_push_error(Nevext.UNCLOSED_MESSAGE)
      return Result.Error, ""
    end
    local b = self.code:byte(self.pos + length)
    if b == BYTE_BACKSLASH then
      length = length + 1
      if self.pos + length > #self.code then
        self:_push_error(Nevext.UNCLOSED_MESSAGE)
        return Result.Error, ""
      end
      -- 被转义的字符若是换行，物理行号同样要推进
      if self.code:byte(self.pos + length) == BYTE_NL then
        self.line = self.line + 1
      end
      length = length + 1
    elseif b == quote_byte then
      break
    else
      if b == BYTE_NL then
        self.line = self.line + 1 -- 引号串可以跨行，行号必须跟着走
      end
      length = length + 1 -- 普通字节必须推进，否则死循环
    end
  end
  local val = self.code:sub(start, start + length - 1)
  self.pos = start + length + 1 -- 越过闭合引号
  return Result.Success, val
end

--- 裸串扫描：遇到空白、逗号、分号、引号或注释起始符即结束。
--- 裸串不展开转义。成功返回 (Result.Success, 内容)；无法推进返回 (Result.Error, "")。
function Lexer:_scan_raw_string()
  local start = self.pos
  local length = 0
  while self.pos + length <= #self.code do
    local b = self.code:byte(self.pos + length)
    if WHITE_BYTES[b] or RAW_END_BYTES[b] then
      break
    end
    if b == BYTE_SLASH and self.pos + length + 1 <= #self.code then
      local nxt = self.code:byte(self.pos + length + 1)
      if nxt == BYTE_SLASH or nxt == BYTE_ASTERISK then
        break -- 裸串在行注释 / 块注释起始处截断
      end
    end
    length = length + 1
  end
  if length <= 0 then
    return Result.Error, ""
  end
  self.pos = self.pos + length
  return Result.Success, self.code:sub(start, start + length - 1)
end

--- 跳过空白与两种注释。未闭合的块注释延伸到输入结束，不是错误。
function Lexer:_skip_ws()
  while self.pos <= #self.code do
    local b = self.code:byte(self.pos)
    if WHITE_BYTES[b] then
      if b == BYTE_NL then
        self.line = self.line + 1
      end
      self.pos = self.pos + 1
    elseif self:_test("//") then
      -- 单行注释：跳到行尾（含换行符本身）
      local nl = self.code:find("\n", self.pos, true)
      if not nl then
        self.pos = #self.code + 1
      else
        self.pos = nl + 1
        self.line = self.line + 1
      end
    elseif self:_test("/*") then
      self:_skip_multiline_comment()
    else
      break
    end
  end
end

function Lexer:_skip_multiline_comment()
  self.pos = self.pos + 2 -- 跳过 /*
  while self.pos <= #self.code do
    if self:_test("*/") then
      self.pos = self.pos + 2
      break
    end
    if self.code:byte(self.pos) == BYTE_NL then
      self.line = self.line + 1
    end
    self.pos = self.pos + 1
  end
end

--- 判断 pos 处是否以 form 开头。
function Lexer:_test(form)
  return self.code:sub(self.pos, self.pos + #form - 1) == form
end

function Lexer:_push_error(msg)
  self.errors[#self.errors + 1] = "line " .. tostring(self.line) .. ": " .. msg
end

-- ------------------------------------------------------------------ 转义展开

--- 展开引号串里的转义序列。与 C++ 的 Parser::escape 一致：
--- 失败时 val 保留失败点之前已展开的部分。
--- 返回 (err, val, reason)，reason 为 "unclosed" | "invalid"，用于错误消息。
function Nevext.expand_escapes(src)
  local out = {}
  local i = 1
  while i <= #src do
    local pos = src:find("\\", i, true)
    if not pos then
      out[#out + 1] = src:sub(i) -- 没有转义字符了
      break
    end
    out[#out + 1] = src:sub(i, pos - 1)
    if pos + 1 > #src then
      -- 输入串末尾是转义字符（正常 token 流里到不了这里，防御性保留）
      return Result.Error, table.concat(out), "unclosed"
    end
    local mapped = ESCAPE_MAP[src:sub(pos + 1, pos + 1)]
    if not mapped then
      return Result.Error, table.concat(out), "invalid"
    end
    out[#out + 1] = mapped
    i = pos + 2
  end
  return Result.Success, table.concat(out), ""
end

-- ------------------------------------------------------------------ 语法分析

local Parser = {}
Parser.__index = Parser
Nevext.Parser = Parser

--- 新建语法分析器并绑定词法器。
function Parser.new(lexer)
  local self = setmetatable({}, Parser)
  self:reset(lexer)
  return self
end

--- 重绑词法器并清空上一轮的错误记录，复用同一对象。
function Parser:reset(lexer)
  self.lexer = lexer
  self.errors = {}
  self._token = nil
  self._failed = false
end

--- 解析整个程序，返回 {err = Result, val = 语句列表}。
--- 语句为 {line = 起始行号, strings = 展开转义后的字符串列表}。
--- pause_when_error 为 true（默认）时遇到第一个错误立即停止；
--- 无论哪种模式，出错前已经解析出的语句都会保留在返回值里。
function Parser:parse_program(pause_when_error)
  if pause_when_error == nil then
    pause_when_error = true
  end
  local result = { err = Result.Success, val = {} }
  self._failed = false
  while true do
    self._token = self.lexer:next_token()
    local ttype = self._token.type
    if ttype == TokenType.EndOfFile then
      break
    elseif ttype == TokenType.Sep then
      -- 空语句，不处理
    elseif ttype == TokenType.RawStr or ttype == TokenType.QuotedStr then
      local stmt = { line = self._token.line, strings = {} }
      stmt.strings[#stmt.strings + 1] = self:_to_literal(self._token)
      self:_scan_rest_of_statement(stmt)
      result.val[#result.val + 1] = stmt
    else
      -- 词法错误（如未闭合字符串）会以 Unexpected token 的形式流到这里，
      -- 必须把返回值置为 Error，否则调用方只看返回值会误判为解析成功。
      self:_unexpected_token_error()
    end
    if pause_when_error and #self.errors > 0 then
      break
    end
  end
  result.err = self._failed and Result.Error or Result.Success
  return result
end

--- 语句内部扫描：直到分号 / EOF / 非法 token。
function Parser:_scan_rest_of_statement(stmt)
  while true do
    self._token = self.lexer:next_token()
    local ttype = self._token.type
    if ttype == TokenType.EndOfFile then
      break -- 最后一个语句可以没有 ; 结尾
    elseif ttype == TokenType.Unexpected then
      self:_unexpected_token_error()
      break
    elseif ttype == TokenType.RawStr or ttype == TokenType.QuotedStr then
      stmt.strings[#stmt.strings + 1] = self:_to_literal(self._token)
    elseif ttype == TokenType.Sep then
      break
    else
      self:_undefined_token_type_error()
      break
    end
  end
end

--- 只有 QuotedStr 才展开转义；失败保留部分结果并记录错误。
function Parser:_to_literal(token)
  if token.type ~= TokenType.QuotedStr then
    return token.literal
  end
  local err, val, reason = Nevext.expand_escapes(token.literal)
  if err ~= Result.Success then
    local msg = reason == "unclosed" and Nevext.UNCLOSED_MESSAGE or Nevext.INVALID_ESCAPE_MESSAGE
    self.errors[#self.errors + 1] = "line " .. tostring(token.line) .. ": " .. msg
    self._failed = true
  end
  return val
end

function Parser:_unexpected_token_error()
  self.errors[#self.errors + 1] =
    "line " .. tostring(self._token.line) .. ": " .. Nevext.UNEXPECTED_MESSAGE
  self._failed = true
end

--- TokenType 只有 5 个取值且全部被处理，这里是防御性兜底。
function Parser:_undefined_token_type_error()
  self.errors[#self.errors + 1] =
    "exception: line "
    .. tostring(self._token.line)
    .. ": undefined token type "
    .. tostring(self._token.type)
  self._failed = true
end

return Nevext
