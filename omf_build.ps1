# One Must Fall 2097 빌드 스크립트

# 빌드 디렉토리 정리 함수
function Clean-BuildDirectory {
    Write-Output "기존 빌드 디렉토리 제거 중..."
    if (Test-Path "build") {
        rm build -Recurse -Force
        Write-Output "빌드 디렉토리 제거 완료"
    } else {
        Write-Output "빌드 디렉토리가 존재하지 않음"
    }
}

# 빌드 디렉토리 생성 함수
function New-BuildDirectory {
    Write-Output "빌드 디렉토리 생성 중..."
    mkdir .\build
    Write-Output "빌드 디렉토리 생성 완료"
}

# CMake 설정 함수
function Invoke-CMakeConfigure {
    Write-Output "CMake 설정 중..."
    $cmakeArgs = @(
        "-B", "build",
        "-S", ".",
        "-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake",
        "-DVCPKG_TARGET_TRIPLET=x64-mingw-static",
        "-DCMAKE_BUILD_TYPE=Release",
        "-G", "MinGW Makefiles"
    )
    & cmake @cmakeArgs
    Write-Output "CMake 설정 완료"
}

# 빌드 실행 함수
function Invoke-Build {
    Write-Output "빌드 실행 중..."
    cmake --build build --config Release
    Write-Output "빌드 실행 완료"
}

# 메인 실행 부분
Write-Output "=== One Must Fall 2097 빌드 시작 ==="

Clean-BuildDirectory
New-BuildDirectory
Invoke-CMakeConfigure
Invoke-Build

Write-Output "=== 빌드 완료 ==="