@echo off
rem Run the Nevext Lua demo program.
setlocal
where lua >nul 2>nul || (
  echo error: lua not found in PATH
  exit /b 1
)
lua "%~dp0demo\demo01.lua" %*
endlocal & exit /b %ERRORLEVEL%
