@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BUILD_NAME=build"
if not "%~1"=="" set "BUILD_NAME=%~1"
set "BUILD_DIR=%SCRIPT_DIR%%BUILD_NAME%"
set "VSDEV=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" (
  echo Visual Studio 2022 Build Tools wurden nicht gefunden.
  exit /b 1
)
call "%VSDEV%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
set "KERNEL_INCLUDE=%SCRIPT_DIR%..\..\..\..\exports\generated"

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 /DUNICODE /D_UNICODE ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%main.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\TATARUS_ResearchUI.exe" ^
  /link user32.lib gdi32.lib comdlg32.lib shell32.lib comctl32.lib /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%engine_tests.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkEngineTests.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%stage20_23_pipeline.cpp" "%SCRIPT_DIR%cognitive_bridge.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGStage20To23.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%research_cli.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkResearch.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%acceptance_tests.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkAcceptance.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%superiority_experiment.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkSuperiority.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%delayed_xor_experiment.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkDelayedXor.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%trace_essential_experiment.cpp" "%SCRIPT_DIR%bio_core.cpp" "%SCRIPT_DIR%classifier.cpp" ^
  /Fe:"%BUILD_DIR%\AGBioNetworkTraceEssential.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%nervous_system_tests.cpp" "%SCRIPT_DIR%cognitive_bridge.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGNervousSystemTests.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%nervous_system_lab.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGNervousSystemLab.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 /DUNICODE /D_UNICODE ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%nervous_system_ui.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\TATARUS.exe" ^
  /link user32.lib gdi32.lib comdlg32.lib shell32.lib /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  "%SCRIPT_DIR%nervous_system_opencl_probe.cpp" ^
  /Fe:"%BUILD_DIR%\AGNervousSystemOpenClProbe.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%representation_research.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGRepresentationResearch.exe"
if errorlevel 1 exit /b 1

cl /nologo /std:c++20 /EHsc /O2 /W4 /permissive- /utf-8 ^
  /Fo"%BUILD_DIR%\\" ^
  /I"%KERNEL_INCLUDE%" ^
  "%SCRIPT_DIR%persistent_ai_trial.cpp" "%SCRIPT_DIR%cognitive_bridge.cpp" "%SCRIPT_DIR%nervous_system.cpp" ^
  /Fe:"%BUILD_DIR%\AGPersistentAITrial.exe"
if errorlevel 1 exit /b 1

echo Build erfolgreich:
echo   %BUILD_DIR%\TATARUS_ResearchUI.exe
echo   %BUILD_DIR%\AGBioNetworkEngineTests.exe
echo   %BUILD_DIR%\AGBioNetworkResearch.exe
echo   %BUILD_DIR%\AGBioNetworkAcceptance.exe
echo   %BUILD_DIR%\AGBioNetworkSuperiority.exe
echo   %BUILD_DIR%\AGBioNetworkDelayedXor.exe
echo   %BUILD_DIR%\AGBioNetworkTraceEssential.exe
echo   %BUILD_DIR%\AGNervousSystemTests.exe
echo   %BUILD_DIR%\AGNervousSystemLab.exe
echo   %BUILD_DIR%\TATARUS.exe
echo   %BUILD_DIR%\AGNervousSystemOpenClProbe.exe
echo   %BUILD_DIR%\AGRepresentationResearch.exe
echo   %BUILD_DIR%\AGPersistentAITrial.exe
echo   %BUILD_DIR%\AGStage20To23.exe
endlocal
