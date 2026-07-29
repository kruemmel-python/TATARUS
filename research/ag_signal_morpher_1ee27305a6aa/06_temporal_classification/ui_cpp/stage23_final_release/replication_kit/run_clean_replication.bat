@echo off
setlocal
set "ROOT=%~dp0..\..\..\..\..\.."
set "SRC=%ROOT%\research\ag_signal_morpher_1ee27305a6aa\06_temporal_classification\ui_cpp"
call "%SRC%\build_ui.bat" build_replication
if errorlevel 1 exit /b 1
pushd "%SRC%"
build_replication\AGRepresentationResearch.exe replication_stage18 --confirm
if errorlevel 1 exit /b 1
build_replication\AGPersistentAITrial.exe replication_stage19 --confirm
if errorlevel 1 exit /b 1
build_replication\AGStage20To23.exe replication_stage20_23 --replication
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
