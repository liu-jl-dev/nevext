@echo off
rem Run the Nevext Rust test suite.
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\test_main.rs)
setlocal
where cargo >nul 2>nul || (
  echo error: cargo not found in PATH
  exit /b 1
)
pushd "%~dp0"
cargo run -q --bin nevext_test -- %*
endlocal & exit /b %ERRORLEVEL%
