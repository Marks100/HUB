@echo off
REM Simple launcher for BuildEnv
REM Author: MS
REM Date: 2025-12-11

echo Starting Build Environment...
REM Change to the /BuildEnv/xBuildEnv directory - single source of truth
cd /d "%~dp0BuildEnv\xBuildEnv"
call BuildEnv.bat
