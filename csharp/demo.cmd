@echo off
rem Run the Nevext C# demo program (requires the .NET SDK).
rem
rem Demo output must stay byte-clean so it can be diffed against the other four
rem implementations, so the build log goes to a temp file and is only echoed
rem when the build fails (SDK prints a completion summary even with -v q).
setlocal
where dotnet >nul 2>nul || (
  echo error: dotnet not found in PATH
  exit /b 1
)

set "LOG=%TEMP%\nevext_demo_build.log"
dotnet build "%~dp0demo\Demo01.csproj" -c Release -v q --nologo > "%LOG%" 2>&1
if errorlevel 1 (
  type "%LOG%"
  del "%LOG%" >nul 2>nul
  exit /b 1
)
del "%LOG%" >nul 2>nul

"%~dp0demo\bin\Release\net10.0\nevext_demo.exe" %*
endlocal & exit /b %ERRORLEVEL%
