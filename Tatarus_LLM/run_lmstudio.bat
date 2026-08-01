@echo off
setlocal
if not exist "%~dp0build\tatarus_llm.exe" call "%~dp0build.bat"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build\tatarus_llm.exe" --provider lmstudio --memory-owner tatarus --episodic-memory anchored --config "%~dp0config\tatarus_llm.example.json"
