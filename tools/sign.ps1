# подписывает файлы Authenticode сертификатом из секретов репозитория (CI на windows-latest).
# SIGN_PFX_BASE64 это pfx в base64, SIGN_PFX_PASSWORD пароль к нему; REQUIRE_SIGNED=1 делает отсутствие
# сертификата ошибкой, так сборка по тегу не выйдет неподписанной
param([Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)][string[]]$Files)

$ErrorActionPreference = 'Stop'
$timestamps = @('http://timestamp.digicert.com', 'http://timestamp.sectigo.com')

if (-not $env:SIGN_PFX_BASE64) {
    if ($env:REQUIRE_SIGNED -eq '1') {
        throw 'SIGN_PFX_BASE64 is not set: a tagged release must be signed'
    }
    Write-Warning 'SIGN_PFX_BASE64 is not set, the files stay unsigned'
    exit 0
}

# последняя установленная версия signtool из Windows SDK
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" |
    Sort-Object FullName | Select-Object -Last 1
if (-not $signtool) {
    throw 'signtool.exe not found'
}

$pfx = Join-Path $env:RUNNER_TEMP 'sign.pfx'
[IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String($env:SIGN_PFX_BASE64))
try {
    foreach ($file in $Files) {
        $signed = $false
        # сервер меток времени бывает недоступен: пробуем следующий
        foreach ($ts in $timestamps) {
            & $signtool.FullName sign /f $pfx /p $env:SIGN_PFX_PASSWORD /fd sha256 /tr $ts /td sha256 /d fringefix $file
            if ($LASTEXITCODE -eq 0) {
                $signed = $true
                break
            }
        }
        if (-not $signed) {
            throw "signtool sign failed for $file"
        }
        & $signtool.FullName verify /pa /v $file
        if ($LASTEXITCODE -ne 0) {
            throw "signature of $file does not verify"
        }
    }
}
finally {
    Remove-Item $pfx -Force -ErrorAction SilentlyContinue
}
