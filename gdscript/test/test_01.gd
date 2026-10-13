extends Control


func _ready() -> void:
	main01()
	get_tree().quit()

func main01():
	var src := '
	// 单行注释
    hello \"world\";
    /*
    多行注释  
    */
    海上生明月 天涯共此时 // 最后一个语句可以没有分号结尾
	'
	var lexer := Nevext.Lexer.new(src)
	var parser := Nevext.Parser.new(lexer)
	var res:Nevext.ParseResult = parser.parse_prog()
	if res.err:
		print("==== Parser errors: ================================")
		for e in parser.errors:
			print(e)
	
	for i in range(len(res.val)):
		var s := res.val[i]
		print("statement ",i+1,' at line ',s.line,':')
		for st in s.strings:
			print('  ', st)







