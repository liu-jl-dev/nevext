@echo off
rem Run the Nevext GDScript demo program.
rem
rem Usage:
rem   demo.cmd                parse the built-in sample
rem   demo.cmd some.txt       parse that file (UTF-8)
rem
rem Set GODOT to override the engine executable (defaults to "godot" on PATH).
rem Godot prints a version banner on stdout; the demo output follows it.
rem Exit code: 0 = parsed, 1 = lexer/parser reported errors, 2 = file unreadable.
rem NOTE: a relative path argument is resolved against this script's directory
rem (the Godot project root), not the caller's cwd -- pass an absolute path.

setlocal
if "%GODOT%"=="" set "GODOT=godot"

pushd "%~dp0"
rem Arguments after "--" reach the script via OS.get_cmdline_user_args()
"%GODOT%" --headless --path . --script res://demo/demo01.gd -- %*
set EXIT_CODE=%ERRORLEVEL%
popd

endlocal & exit /b %EXIT_CODE%
