@echo off
rem Run the Nevext Python test suite.
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\test_main.py)
setlocal
where python >nul 2>nul || (
  echo error: python not found in PATH
  exit /b 1
)
python "%~dp0test\test_main.py" %*
endlocal & exit /b %ERRORLEVEL%
