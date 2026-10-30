@echo off
rem Run the Nevext JavaScript test suite.
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\test_main.js)
setlocal
node "%~dp0test\test_main.js" %*
endlocal & exit /b %ERRORLEVEL%
