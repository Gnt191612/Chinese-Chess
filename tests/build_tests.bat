@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer vswhere.exe was not found.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
    echo Visual Studio C++ tools were not found.
    exit /b 1
)
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /I src\include tests\LogicTests.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp src\core\GameLogic.cpp /Fe:tests\LogicTests.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /I src\include tests\TimingTest.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp /Fe:tests\TimingTest.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /utf-8 /wd4828 /I src\include tests\RulesAudit.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp src\core\GameLogic.cpp /Fe:tests\RulesAudit.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /utf-8 /wd4828 /I src\include tests\ShortcutTests.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp src\core\GameLogic.cpp /Fe:tests\ShortcutTests.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /utf-8 /wd4828 /I src\include tests\EvaluationTests.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp /Fe:tests\EvaluationTests.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /utf-8 /wd4828 /I src\include tests\OpeningDiagnostic.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp /Fe:tests\OpeningDiagnostic.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /utf-8 /wd4828 /DUNICODE /D_UNICODE /I src\include tests\ExperiencePresentationTests.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp /Fe:tests\ExperiencePresentationTests.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /utf-8 /wd4828 /I src\include tests\SessionRulesTests.cpp src\core\ChessBoard.cpp src\core\ChessPiece.cpp src\core\AIEngine.cpp src\core\ExperienceBook.cpp src\core\GameLogic.cpp /Fe:tests\SessionRulesTests.exe /link /LIBPATH:"%VSROOT%\VC\Auxiliary\VS\lib\x64" user32.lib gdi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
endlocal
