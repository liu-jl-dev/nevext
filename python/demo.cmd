@echo off
rem Run the Nevext Python demo program.
setlocal
where python >nul 2>nul || (
  echo error: python not found in PATH
  exit /b 1
)
python "%~dp0demo\demo01.py" %*
endlocal & exit /b %ERRORLEVEL%
