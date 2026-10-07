@echo off
setlocal
cd /d "%~dp0..\.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT exit /b 1
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
if not exist build\training-probe mkdir build\training-probe
cl /nologo /EHsc /I src\include tools\training\PositionProbe.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp /Fe:build\training-probe\PositionProbe.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
exit /b %errorlevel%
