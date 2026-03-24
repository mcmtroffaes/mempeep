$BuildDir = "$PSScriptRoot/out/build/coverage"
$ProfilesDir = "$BuildDir/profiles"
$ReportDir = "$BuildDir/report"
$MergedProfile = "$BuildDir/merged.profdata"

# Clean old data
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $ProfilesDir
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $ReportDir
Remove-Item -Force -ErrorAction SilentlyContinue $MergedProfile

# Run tests to generate profiles
ctest --preset coverage
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Merge profiles
$ProfileFiles = @(Get-ChildItem "$ProfilesDir/*.profraw" | ForEach-Object { $_.FullName })
llvm-profdata merge @ProfileFiles -o $MergedProfile
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$TestBinaries = Get-ChildItem "$BuildDir/cpp/test/test_*.exe"
$Object = $TestBinaries | Select-Object -First 1
$ExtraObjects = @($TestBinaries | Select-Object -Skip 1 | ForEach-Object { "-object", $_.FullName })

# Generate report
llvm-cov show $Object.FullName `
    @ExtraObjects `
    -instr-profile $MergedProfile `
    -sources "$PSScriptRoot/cpp/include/mempeep" `
    -sources "$PSScriptRoot/cpp/examples/" `
    -format=html `
    -output-dir $ReportDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Report written to $ReportDir/index.html"