# PowerShell build script for producer.cpp

Write-Host "Compiling examples..." -ForegroundColor Cyan

# Compile the original examples
g++ -o producer.exe examples\producer.cpp -I include -std=c++17
g++ -o consumer.exe examples\consumer.cpp -I include -std=c++17

if ($LASTEXITCODE -eq 0) {
    Write-Host "Compilation successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Available executables:" -ForegroundColor Yellow
    Write-Host "  .\producer.exe / .\consumer.exe  - File-based queue"
    Write-Host "  .\producer_step1.exe / .\consumer_step1.exe  - Shared memory learning (Step 1)"
} else {
    Write-Host "Compilation failed!" -ForegroundColor Red
    exit 1
}
