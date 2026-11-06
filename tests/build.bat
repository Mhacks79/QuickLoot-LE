@echo off
rem Builds the offline tests against the plugin source (docs/BUILDING.md).
rem   VCVARS  path to vcvars32.bat (MSVC v143 was used)
rem   SDK     SKSE 1.7.3 source root; by default this repository sits in <SDK>\skse_plugins\QuickLootIELE
cd /d "%~dp0"
if "%VCVARS%"=="" set VCVARS=C:\BuildTools2022\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul
if "%SDK%"=="" set SDK=%~dp0..\..\..
set SRC=%~dp0..\src
if not "%QL_SRC%"=="" set SRC=%QL_SRC%
set OUT=build
if not "%QL_OUT%"=="" set OUT=%QL_OUT%
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /EHa /MT /O2 /std:c++17 /W3 /wd4244 /wd4267 /wd4996 /D_CRT_SECURE_NO_WARNINGS /DRUNTIME /DRUNTIME_VERSION=0x09200000 ^
 /FI common/IPrefix.h /I "%SRC%" /I "%SDK%" /I "%SDK%\skse" /I "%SDK%\skse\skse" ^
 Tests.cpp "%SRC%\Settings.cpp" "%SRC%\Input.cpp" "%SRC%\Log.cpp" "%SDK%\skse\skse\InputMap.cpp" ^
 /Fo"%OUT%\\" /Fe"%OUT%\QuickLootTests.exe" /link "%SDK%\common\Builds\Release\common_vc9.lib" shell32.lib user32.lib
exit /b %ERRORLEVEL%
