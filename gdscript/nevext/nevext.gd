class_name Nevext # 默认继承 RefCounted

enum TokenType {
	Unexpected, EOF, Str, Sep,
}

class Token: # 默认继承 RefCounted
	var type:TokenType
	var line:int
	var literal:String

class Result:
	var err:int = OK
	var val
	const OK = 0
	const Err = 1

const white_chars = { # in 关键字为从 key 中寻找
	' ':null,',':null,'\t':null,'\v':null,'\r':null,'\n':null,'\f':null,'\b':null,
}

class Lexer:
	var escape_func:Callable = Nevext.defalt_escape
	var code:String
	var line:int = 1
	func _init(code_:String="") -> void:
		reset(code_)
	func reset(code_:String):
		self.code = code_; line = 1
	func next_token() -> Token:
		var token:Token = Token.new()
		_skip_ws(); token.line = line
		if len(code) <= 0:
			token.type = TokenType.EOF
			token.literal = '<EOF>'
		elif code[0] == ';':
			token.type = TokenType.Sep
			token.literal = ';'
			code = code.substr(1)
		else:
			var scan_res:Result = _scan_string()
			if scan_res.err:
				token.type = TokenType.Unexpected
				token.literal = "< unclosed string >"
			else:
				var esc_res:Result = escape_func.call(scan_res.val)
				if esc_res.err:
					token.type = TokenType.Unexpected
					token.literal = '< unexpected escape char >'
				else:
					token.type = TokenType.Str
					token.literal = esc_res.val
		return token
	func _scan_string() -> Result:
		var o:Result = Result.new()
		if code[0] == '\'':
			var res:Result = _scan_quote_string("'")
			if res.err: o.err = Result.Err
			else: o.val = res.val
		elif code[0] == '"':
			var res:Result = _scan_quote_string('"')
			if res.err: o.err = Result.Err
			else: o.val = res.val
		else:
			var res:Result = _scan_raw_string()
			if res.err: o.err = Result.Err
			else: o.val = res.val
		return o
	func _scan_quote_string(quote_char:String) -> Result:
		var res = Result.new(); code = code.substr(1); var strlen:int = 0
		while true:
			if len(code) <= strlen: # 字符串未结束
				res.err = Result.Err
				break
			if _test('\\'+quote_char, strlen):
				strlen += 2
				continue
			if code[strlen] == quote_char: break
			else:
				if code[strlen] == '\n': line += 1
				strlen += 1
		res.val = code.substr(0, strlen)
		code = code.substr(strlen + 1) # 跳过最后的引号
		return res
	func _scan_raw_string() -> Result:
		var literal := ''
		while len(code) > 0:
			if code[0] in Nevext.white_chars or code[0] == ';':
				break
			literal += code[0]; code = code.substr(1) # raw 字符串不可能包含 '\n'，不用 line += 1 
		var res = Result.new(); res.val = literal
		return res
	func _skip_ws():
		while len(code) > 0:
			if code[0] in Nevext.white_chars:
				if code[0] == '\n': line += 1
				code = code.substr(1)
			elif _test('//'): code = code.substr(code.find('\n'))
			elif _test('/*'): _skip_multiline_comment()
			else:
				break
	func _skip_multiline_comment():
		code = code.substr(2)
		while len(code) > 0:
			if _test('*/'): code = code.substr(2); break
			if code[0] == '\n': line += 1
			code = code.substr(1)
	func _test(form:String, offset:int=0) -> bool:
		var c := code.substr(offset)
		if len(c) < len(form): return false
		return c.substr(0, len(form)) == form
	

static func null_escape(s:String) -> Result:
	var res := Result.new(); res.val = s
	return res

const escape_map = {
	"'":"'",'"':'"','\\':'\\',
	'b':'\b','f':'\f','n':'\n',
	'r':'\r','t':'\t','v':'\v',
}
static func defalt_escape(s:String) -> Result:
	var res:Result = Result.new(); res.val = ''
	while len(s) > 0:
		var pos = s.find('\\')
		if pos < 0: # 没有 \
			res.val += s
			break
		res.val += s.substr(0, pos); s = s.substr(pos)
		if not (s[1] in escape_map.keys()):
			res.err = Result.Err
			break
		else:
			res.val += escape_map[s[1]]
			s = s.substr(pos + 2)
	return res


''' 语法分析 '''
class Statement:
	var line:int
	var strings:Array[String]
	func _to_string() -> String:
		return '(Statement line:'+str(line)+' '+str(strings)+')'
class ParseResult:
	var err:int
	var val:Array[Statement]
class Parser:
	var lexer:Lexer
	var errors:Array[String]
	
	var _token:Token
	func _init(lexer_:Lexer=null) -> void:
		reset(lexer_)
	func reset(lexer_:Lexer):
		lexer = lexer_
		errors = []
	func parse_prog(pause_when_error:bool=true) -> ParseResult:
		var res := ParseResult.new(); res.val = []
		while true:
			_token = lexer.next_token()
			if _token.type == TokenType.EOF: break
			elif _token.type == TokenType.Sep: # 空语句
				pass
			elif _token.type == TokenType.Unexpected:
				_unexpected_token_error()
				res.err = Result.Err
			elif _token.type == TokenType.Str:
				var stmt_res:Result = _parse_statement()
				if stmt_res.err: res.err = Result.Err
				res.val.append(stmt_res.val)
			else: _undefined_tokentype_err()
			if pause_when_error and len(errors) > 0: break
		return res
	func _parse_statement() -> Result:
		var res:Result = Result.new()
		var stmt:Statement = Statement.new(); res.val = stmt
		stmt.line = _token.line
		stmt.strings = [_token.literal]
		while true:
			_token = lexer.next_token()
			if _token.type == TokenType.EOF: break # 允许最后一个语句没有分号结尾
			elif _token.type == TokenType.Unexpected:
				_unexpected_token_error()
				res.err = Result.Err; break
			elif _token.type == TokenType.Str:
				stmt.strings.append(_token.literal)
			elif _token.type == TokenType.Sep: break
			else: _undefined_tokentype_err(); break
		return res
	func _undefined_tokentype_err():
		errors.append('exception: line '+str(_token.line)
			+': undefined token type '+ str(_token.type))
	func _unexpected_token_error():
		errors.append('exception: line '+str(_token.line)+': '+ _token.literal)
	

















