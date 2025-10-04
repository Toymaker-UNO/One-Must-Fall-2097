# Simple Direct Build Script for One Must Fall 2097 (C++ Version)
# CMake 없이 직접 GCC로 C++ 빌드

Write-Host "=== C++ Build Started ===" -ForegroundColor Magenta

# 설정
$SRC_DIR = "src"
$BUILD_DIR = "build"
$RESOURCES_DIR = "resources"
$GAME_RESOURCES_DIR = "resources/game_resources"
$VCPKG_LIB_DIR = "lib/bin"
$VCPKG_INCLUDE_DIR = "lib/include"
$OUTPUT_NAME = "openomf.exe"

# 컴파일러 설정
$CC = "gcc" # Use C compiler for source files
$CXX = "g++" # Use C++ compiler for linking
$CFLAGS = @(
    "-std=c11", # Use C11 standard for C files
    "-O2",
    "-w", # Disable all warnings
    "-I$SRC_DIR",
    "-I$SRC_DIR/vendored",
    "-I$VCPKG_INCLUDE_DIR",
    "-I$VCPKG_INCLUDE_DIR/SDL2",
    "-DENABLE_SDL_AUDIO_BACKEND",
    "-DENABLE_NULL_AUDIO_BACKEND",
    "-DENABLE_OPENGL3_RENDERER",
    "-DENABLE_NULL_RENDERER",
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
    "-fpermissive", # Allow implicit void* conversions
    "-Wno-jump-misses-init", # Allow goto across variable initialization
    "-include", "stdint.h", # Include standard integer types
    "-include", "stddef.h", # Include size_t
    "-include", "string.h", # Include memset
    "-include", "stdbool.h" # Include bool type
)

$CXXFLAGS = @(
    "-std=c++17", # Use C++17 standard for C++ files
    "-O2",
    "-w", # Disable all warnings
    "-I$SRC_DIR",
    "-I$SRC_DIR/vendored",
    "-I$VCPKG_INCLUDE_DIR",
    "-I$VCPKG_INCLUDE_DIR/SDL2",
    "-DENABLE_SDL_AUDIO_BACKEND",
    "-DENABLE_NULL_AUDIO_BACKEND",
    "-DENABLE_OPENGL3_RENDERER",
    "-DENABLE_NULL_RENDERER",
    "-DSDL_MAIN_HANDLED",
    "-DUSE_LIBPNG=1",
    "-DPNG_FOUND=1",
    "-DUSE_OPUSFILE=1",
    "-DLIBXMP_STATIC",
    "-DXMP_STATIC",
    "-DV_MAJOR=0",
    "-DV_MINOR=0",
    "-DV_PATCH=0",
    "-DPERROR=printf"
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
$C_FILES = Get-ChildItem -Path $SRC_DIR -Recurse -Filter "*.c" | ForEach-Object { $_.FullName }
$CPP_FILES = Get-ChildItem -Path $SRC_DIR -Recurse -Filter "*.cpp" | ForEach-Object { $_.FullName }
Write-Host "Found $($C_FILES.Count) C source files" -ForegroundColor Green
Write-Host "Found $($CPP_FILES.Count) C++ source files" -ForegroundColor Green

# 컴파일
Write-Host "Compiling..." -ForegroundColor Yellow

# 먼저 모든 .c 파일을 .o 오브젝트 파일로 컴파일
$OBJECT_FILES = @()

# C 파일들 컴파일
foreach ($source_file in $C_FILES) {
    $object_file = $source_file -replace "\.c$", ".o"
    $object_file = $object_file -replace "src\\", "$BUILD_DIR\\"
    $object_dir = Split-Path $object_file -Parent
    if (!(Test-Path $object_dir)) {
        New-Item -ItemType Directory -Path $object_dir -Force | Out-Null
    }
    
    $compile_args = $CFLAGS + $source_file + "-c", "-o", $object_file
    Write-Host "Compiling C: $source_file..." -ForegroundColor Cyan
    & $CC @compile_args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to compile $source_file" -ForegroundColor Red
        exit 1
    }
    $OBJECT_FILES += $object_file
}

# C++ 파일들 컴파일
foreach ($source_file in $CPP_FILES) {
    $object_file = $source_file -replace "\.cpp$", ".o"
    $object_file = $object_file -replace "src\\", "$BUILD_DIR\\"
    $object_dir = Split-Path $object_file -Parent
    if (!(Test-Path $object_dir)) {
        New-Item -ItemType Directory -Path $object_dir -Force | Out-Null
    }
    
    $compile_args = $CXXFLAGS + $source_file + "-c", "-o", $object_file
    Write-Host "Compiling C++: $source_file..." -ForegroundColor Cyan
    & $CXX @compile_args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to compile $source_file" -ForegroundColor Red
        exit 1
    }
    $OBJECT_FILES += $object_file
}

# 오브젝트 파일들을 C++ 링커로 링크
Write-Host "Linking..." -ForegroundColor Yellow
$LINK_ARGS = $OBJECT_FILES + $LDFLAGS + "-o", "$BUILD_DIR/$OUTPUT_NAME"

try {
    & $CXX @LINK_ARGS
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

# 셰이더 파일 복사
Write-Host "Copying shader files..." -ForegroundColor Yellow
if (Test-Path "resources/shaders") {
    $SHADER_BUILD_DIR = "$BUILD_DIR/shaders"
    New-Item -ItemType Directory -Path $SHADER_BUILD_DIR -Force | Out-Null
    
    $SHADER_FILES = Get-ChildItem -Path "resources/shaders" -File
    $COPIED_COUNT = 0
    
    foreach ($file in $SHADER_FILES) {
        try {
            Copy-Item -Path $file.FullName -Destination "$SHADER_BUILD_DIR/$($file.Name)" -Force
            $COPIED_COUNT++
        } catch {
            Write-Host "Failed to copy shader: $($file.Name)" -ForegroundColor Red
        }
    }
    
    Write-Host "Copied $COPIED_COUNT shader files" -ForegroundColor Green
} else {
    Write-Host "Shaders directory not found!" -ForegroundColor Red
}

Write-Host "=== Build Completed ===" -ForegroundColor Magenta
Write-Host "Executable: $BUILD_DIR/$OUTPUT_NAME" -ForegroundColor Cyan
