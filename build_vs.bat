@echo off
set BUILD_DIR=_build

if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo CMake generation failed!
    exit /b %errorlevel%
)

cmake --build %BUILD_DIR% --config Debug
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b %errorlevel%
)

echo Build successful!
