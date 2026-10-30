@echo off
rem Run the Nevext C# test suite (requires the .NET SDK).
rem Usage: test.cmd [name-substring | -v | --dump]   (see test\TestMain.cs)
setlocal
where dotnet >nul 2>nul || (
  echo error: dotnet not found in PATH
  exit /b 1
)
rem Build first, then run the exe directly: keeps --dump output free of build noise.
dotnet build "%~dp0nevext.csproj" -c Release -v q --nologo || exit /b 1
"%~dp0bin\Release\net10.0\nevext_test.exe" %*
endlocal & exit /b %ERRORLEVEL%
