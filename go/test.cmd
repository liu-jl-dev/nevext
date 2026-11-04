@echo off
rem Run the Nevext Go test suite.
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\test_main.go)
setlocal
where go >nul 2>nul || (
  echo error: go not found in PATH
  exit /b 1
)
rem go run needs the module root as cwd (go.mod lives in this directory);
rem test arguments contain no file paths (substring / -v / --dump only),
rem so changing cwd here is safe.
pushd "%~dp0"
go run ./test %*
endlocal & exit /b %ERRORLEVEL%
