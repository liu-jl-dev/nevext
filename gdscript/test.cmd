@echo off
rem Run Nevext GDScript unit tests.
rem
rem Usage:
rem   test.cmd                 run all cases
rem   test.cmd Conformance     only the cross-implementation corpus
rem   test.cmd Impl            only the GDScript-specific cases
rem   test.cmd --dump          print corpus results for diffing against C++
rem
rem Set GODOT to override the engine executable (defaults to "godot" on PATH).
rem Exit code: 0 = all cases passed, 1 = there were failures.

setlocal
if "%GODOT%"=="" set "GODOT=godot"

pushd "%~dp0"
"%GODOT%" --headless --path . --script res://test/test_main.gd -- %*
set EXIT_CODE=%ERRORLEVEL%
popd

endlocal & exit /b %EXIT_CODE%
