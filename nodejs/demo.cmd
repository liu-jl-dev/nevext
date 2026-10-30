@echo off
rem Run the Nevext JavaScript demo program.
setlocal
node "%~dp0demo\demo01.js" %*
endlocal & exit /b %ERRORLEVEL%
