@echo off
rem Run the Nevext Rust demo program.
rem Builds first, then runs the exe directly so the demo process runs in the
rem caller's cwd (relative file-path arguments keep working), and build
rem chatter never pollutes stdout/stderr.
setlocal
where cargo >nul 2>nul || (
  echo error: cargo not found in PATH
  exit /b 1
)
pushd "%~dp0"
cargo build -q --bin demo01
popd
if errorlevel 1 exit /b 1
"%~dp0target\debug\demo01.exe" %*
endlocal & exit /b %ERRORLEVEL%
