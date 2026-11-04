@echo off
rem Run the Nevext Lua test suite.
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\test_main.lua)
setlocal
where lua >nul 2>nul || (
  echo error: lua not found in PATH
  exit /b 1
)
lua "%~dp0test\test_main.lua" %*
endlocal & exit /b %ERRORLEVEL%
