# One Must Fall 2097 Build Script

# Clean build directory function
function Clean-BuildDirectory {
    Write-Host "Removing existing build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        rm build -Recurse -Force
        Write-Host "Build directory removed successfully" -ForegroundColor Green
    } else {
        Write-Host "Build directory does not exist" -ForegroundColor Cyan
    }
}

# Create build directory function
function New-BuildDirectory {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    mkdir .\build
    Write-Host "Build directory created successfully" -ForegroundColor Green
}

# CMake configuration function
function Invoke-CMakeConfigure {
    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    $cmakeArgs = @(
        "-B", "build",
        "-S", ".",
        "-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake",
        "-DVCPKG_TARGET_TRIPLET=x64-mingw-static",
        "-DCMAKE_BUILD_TYPE=Release",
        "-G", "MinGW Makefiles"
    )
    & cmake @cmakeArgs
    Write-Host "CMake configuration completed" -ForegroundColor Green
}

# Build execution function
function Invoke-Build {
    Write-Host "Building project..." -ForegroundColor Yellow
    cmake --build build --config Release
    Write-Host "Build completed successfully" -ForegroundColor Green
}

# Game resources copy function
function Copy-GameResources {
    Write-Host "Copying game resources..." -ForegroundColor Yellow
    
    # Check source directory
    if (-not (Test-Path "game_resources")) {
        Write-Host "game_resources directory not found!" -ForegroundColor Red
        return
    }
    
    # Create target directory
    $targetDir = "build\resource"
    if (-not (Test-Path $targetDir)) {
        Write-Host "Creating resource directory: $targetDir" -ForegroundColor Cyan
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    }
    
    # Copy files
    $sourceFiles = Get-ChildItem -Path "game_resources" -File
    $copiedCount = 0
    
    foreach ($file in $sourceFiles) {
        $destination = Join-Path $targetDir $file.Name
        try {
            Copy-Item -Path $file.FullName -Destination $destination -Force
            Write-Host "Copied: $($file.Name)" -ForegroundColor Green
            $copiedCount++
        }
        catch {
            Write-Host "Failed to copy: $($file.Name) - $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    
    Write-Host "Game resources copy completed: $copiedCount files" -ForegroundColor Green
}

# Main execution section
Write-Host "=== One Must Fall 2097 Build Started ===" -ForegroundColor Magenta

Clean-BuildDirectory
New-BuildDirectory
Invoke-CMakeConfigure
Invoke-Build
#Copy-GameResources

Write-Host "=== Build Completed ===" -ForegroundColor Magenta