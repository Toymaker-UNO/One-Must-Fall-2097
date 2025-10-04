# Simple Direct Build Script for One Must Fall 2097
# CMake 없이 직접 GCC로 빌드

Write-Host "=== Simple Build Started ===" -ForegroundColor Magenta

# 설정
$SRC_DIR = "src"
$BUILD_DIR = "build"
$RESOURCES_DIR = "resources"
$GAME_RESOURCES_DIR = "resources/game_resources"
$VCPKG_LIB_DIR = "vcpkg_installed/x64-mingw-static/lib"
$VCPKG_INCLUDE_DIR = "vcpkg_installed/x64-mingw-static/include"
$OUTPUT_NAME = "openomf.exe"

# 컴파일러 설정
$CC = "gcc" # Use C compiler
$CFLAGS = @(
    "-std=c11",
    "-O2",
    "-w", # Disable all warnings
    "-I$SRC_DIR",
    "-I$SRC_DIR/vendored",
    "-I$VCPKG_INCLUDE_DIR",
    "-I$VCPKG_INCLUDE_DIR/SDL2",
    "-DENABLE_SDL_AUDIO_BACKEND",
    "-DENABLE_NULL_AUDIO_BACKEND",
    "-DSDL_MAIN_HANDLED",
    "-DUSE_LIBPNG=1",
    "-DPNG_FOUND=1",
    "-DUSE_OPUSFILE=1",
    "-DLIBXMP_STATIC",
    "-DXMP_STATIC",
    "-DV_MAJOR=0",
    "-DV_MINOR=0", 
    "-DV_PATCH=0",
    "-DPERROR=printf",
    "-include", "stdbool.h",
    "-include", "stddef.h",
    "-include", "stdint.h",
    "-include", "stdio.h",
    "-include", "ctype.h",
    "-include", "string.h"
)

# 링커 설정
$LDFLAGS = @(
    "-L$VCPKG_LIB_DIR",
    "-lSDL2",
    "-lSDL2_mixer",
    "-lxmp",
    "-lepoxy",
    "-lenet",
    "-lconfuse",
    "-lpng",
    "-lzlib",
    "-lopusfile",
    "-lopus",
    "-lvorbisfile",
    "-lvorbisenc", 
    "-lvorbis",
    "-logg",
    "-lm",
    "-lwinmm",
    "-lws2_32",
    "-lole32",
    "-loleaut32",
    "-lsetupapi",
    "-limm32",
    "-lgdi32",
    "-luser32",
    "-ladvapi32",
    "-lshell32",
    "-lversion", # For version info functions
    "-lshlwapi", # For PathFileExistsA function
    "-lmingw32", # For MinGW specific linking
    "-mconsole" # Force console application
)

# 빌드 디렉토리 정리 및 생성
Write-Host "Cleaning build directory..." -ForegroundColor Yellow
if (Test-Path $BUILD_DIR) {
    Remove-Item -Recurse -Force $BUILD_DIR
}
New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null

# 소스 파일 수집
Write-Host "Collecting source files..." -ForegroundColor Yellow
$SOURCE_FILES = Get-ChildItem -Path $SRC_DIR -Recurse -Filter "*.c" | ForEach-Object { $_.FullName }
Write-Host "Found $($SOURCE_FILES.Count) source files" -ForegroundColor Green

# 컴파일
Write-Host "Compiling..." -ForegroundColor Yellow
$COMPILE_ARGS = $CFLAGS + $SOURCE_FILES + $LDFLAGS + "-o", "$BUILD_DIR/$OUTPUT_NAME"

try {
    & $CC @COMPILE_ARGS
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful!" -ForegroundColor Green
    } else {
        Write-Host "Compilation failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "Compilation error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# 리소스 복사
Write-Host "Copying game resources..." -ForegroundColor Yellow
if (Test-Path $GAME_RESOURCES_DIR) {
    $RESOURCE_BUILD_DIR = "$BUILD_DIR/resources"
    New-Item -ItemType Directory -Path $RESOURCE_BUILD_DIR -Force | Out-Null
    
    $RESOURCE_FILES = Get-ChildItem -Path $GAME_RESOURCES_DIR -File
    $COPIED_COUNT = 0
    
    foreach ($file in $RESOURCE_FILES) {
        try {
            Copy-Item -Path $file.FullName -Destination "$RESOURCE_BUILD_DIR/$($file.Name)" -Force
            $COPIED_COUNT++
        } catch {
            Write-Host "Failed to copy: $($file.Name)" -ForegroundColor Red
        }
    }
    
    Write-Host "Copied $COPIED_COUNT resource files" -ForegroundColor Green
} else {
    Write-Host "Game resources directory not found!" -ForegroundColor Red
}

Write-Host "=== Build Completed ===" -ForegroundColor Magenta
Write-Host "Executable: $BUILD_DIR/$OUTPUT_NAME" -ForegroundColor Cyan
