@echo off
rem Run the Nevext C# test suite (requires the .NET SDK).
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\TestMain.cs)
setlocal
where dotnet >nul 2>nul || (
  echo error: dotnet not found in PATH
  exit /b 1
)
rem Build first, then run the exe directly. The build log goes to a temp file and
rem is echoed only on failure: the SDK prints a completion summary even with -v q,
rem which would otherwise pollute the --dump output used by the cross-impl gate.
set "LOG=%TEMP%\nevext_test_build.log"
dotnet build "%~dp0nevext.csproj" -c Release -v q --nologo > "%LOG%" 2>&1
if errorlevel 1 (
  type "%LOG%"
  del "%LOG%" >nul 2>nul
  exit /b 1
)
del "%LOG%" >nul 2>nul

"%~dp0bin\Release\net10.0\nevext_test.exe" %*
endlocal & exit /b %ERRORLEVEL%
