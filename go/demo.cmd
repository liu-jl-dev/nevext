@echo off
rem Run the Nevext Go demo program.
rem Builds to a temp exe first so the demo process runs in the caller's cwd
rem (relative file-path arguments keep working).
setlocal
where go >nul 2>nul || (
  echo error: go not found in PATH
  exit /b 1
)
set "EXE=%TEMP%\nevext_demo01_go.exe"
pushd "%~dp0"
go build -o "%EXE%" ./demo
popd
if errorlevel 1 exit /b 1
"%EXE%" %*
endlocal & exit /b %ERRORLEVEL%
