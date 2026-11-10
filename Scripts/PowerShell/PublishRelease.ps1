param(
    [string]$Tag,
    [string]$Repository = 'isrepeat/SolutionInjector',
    [string]$Target = 'refs/heads/Last',
    [string]$StableTag = 'Last',
    [string]$ProjectPath,
    [string]$RuntimeDirectory,
    [string]$Title,
    [switch]$Draft,
    [switch]$PrepareOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function InvokeChecked([string]$Command, [string[]]$Arguments) {
    $result = & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE." }
    return $result
}

try {
    # 1. Выбираем следующую patch-версию; учитываем черновики и уже занятые теги.
    if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw 'Expected GitHub owner/repository.' }
    if (-not $Tag) {
        $releaseJson = InvokeChecked 'gh' @('release', 'list', '--repo', $Repository, '--limit', '1000', '--json', 'tagName')
        $releaseTags = @($releaseJson | ConvertFrom-Json | ForEach-Object { $_.tagName })
        $remoteTags = @(InvokeChecked 'git' @('ls-remote', '--tags', "https://github.com/$Repository.git") |
            ForEach-Object { if ($_ -match 'refs/tags/(v\d+\.\d+\.\d+)$') { $Matches[1] } })
        $versions = @(@($releaseTags) + @($remoteTags) |
            Where-Object { $_ -match '^v\d+\.\d+\.\d+$' } |
            ForEach-Object { [version]$_.Substring(1) } | Sort-Object -Descending)
        $Tag = if ($versions.Count) {
            'v{0}.{1}.{2}' -f $versions[0].Major, $versions[0].Minor, ($versions[0].Build + 1)
        } else {
            'v1.0.0'
        }
    }
    # Ограничиваем имя также для безопасного использования в пути архива.
    if ($Tag -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw 'Invalid release tag.' }
    if ($StableTag -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$' -or $StableTag -eq $Tag) { throw 'Invalid or conflicting stable tag.' }
    if ($Repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw 'Expected GitHub owner/repository.' }
    $commit = (InvokeChecked 'git' @('-C', $repositoryRoot, 'rev-parse', '--verify', "$Target^{commit}")).Trim()

    # 2. Пересобираем выбранный проект в Release x64; старый EXE не используется при ошибке сборки.
    $headCommit = (InvokeChecked 'git' @('-C', $repositoryRoot, 'rev-parse', '--verify', 'HEAD')).Trim()
    if ($headCommit -ne $commit) { throw 'Target must resolve to the current HEAD because the current working tree is built.' }
    $workingTreeModified = @(InvokeChecked 'git' @('-C', $repositoryRoot, 'status', '--porcelain', '--untracked-files=normal')).Count -gt 0
    if (-not $ProjectPath) { $ProjectPath = 'SolutionInjector/SolutionInjector.vcxproj' }
    if (-not [IO.Path]::IsPathRooted($ProjectPath)) { $ProjectPath = Join-Path $repositoryRoot $ProjectPath }
    $ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    $repositoryPrefix = $repositoryRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $ProjectPath.StartsWith($repositoryPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'ProjectPath must belong to the SolutionInjector repository.' }
    if ([IO.Path]::GetExtension($ProjectPath) -ne '.vcxproj') { throw 'ProjectPath must point to a C++ .vcxproj project.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio MSBuild with C++ tools is required.' }
    $msbuildCandidates = @(InvokeChecked $vswhere @('-latest', '-products', '*', '-requires', 'Microsoft.Component.MSBuild', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-find', 'MSBuild/Current/Bin/MSBuild.exe'))
    if (-not $msbuildCandidates.Count) { throw 'MSBuild with C++ tools was not found.' }
    $msbuild = $msbuildCandidates[0]
    Write-Host "Building Release x64: $ProjectPath"
    & $msbuild $ProjectPath -nologo -m -t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=$repositoryRoot/"
    if ($LASTEXITCODE -ne 0) { throw "Release build failed with exit code $LASTEXITCODE. Nothing was published." }
    $targetPath = InvokeChecked $msbuild @($ProjectPath, '-nologo', '-verbosity:quiet', '-getProperty:TargetPath', '/p:Configuration=Release', '/p:Platform=x64', "/p:SolutionDir=$repositoryRoot/")
    $executablePath = ($targetPath -join "`n").Trim()
    if (-not $executablePath) { throw 'MSBuild did not return TargetPath.' }
    $executable = (Resolve-Path -LiteralPath $executablePath).Path
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Executable not found.' }
    # Проверяем PE-заголовок: этот архив предназначен только для Windows x64.
    $stream = [IO.File]::OpenRead($executable)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Invalid executable header.' }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw 'Expected Windows x64 PE executable.'
        }
    }
    finally { $reader.Dispose() }

    if (-not $RuntimeDirectory) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Specify RuntimeDirectory containing x64 Visual C++ Runtime DLLs.' }
        $installation = (InvokeChecked $vswhere @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'))
        if (-not $installation) { throw 'Visual C++ installation not found. Specify RuntimeDirectory.' }
        $redistRoot = Join-Path $installation 'VC/Redist/MSVC'
        $runtimeCandidates = @(Get-ChildItem -LiteralPath $redistRoot -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' })
        if (-not $runtimeCandidates.Count) { throw 'x64 Visual C++ Runtime not found. Specify RuntimeDirectory.' }
        $RuntimeDirectory = $runtimeCandidates[0].FullName
    }
    $runtimePath = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
    foreach ($requiredDll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $runtimePath $requiredDll) -PathType Leaf)) {
            throw "Runtime DLL not found: $requiredDll"
        }
    }

    # 3. Подготавливаем отдельный архив и контрольные суммы до запроса публикации.
    $outputDirectory = Join-Path $repositoryRoot ('!artifacts/releases/' + $Tag + '/' + [Guid]::NewGuid().ToString('N'))
    $packageDirectory = Join-Path $outputDirectory 'package'
    New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
    Copy-Item -LiteralPath $executable -Destination (Join-Path $packageDirectory 'SolutionInjector.exe')
    Get-ChildItem -LiteralPath $runtimePath -Filter '*.dll' -File |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $packageDirectory }
    $archive = Join-Path $outputDirectory ("SolutionInjector-$Tag-windows-x64.zip")
    Compress-Archive -Path (Join-Path $packageDirectory '*') -DestinationPath $archive
    $exeHash = (Get-FileHash -LiteralPath (Join-Path $packageDirectory 'SolutionInjector.exe') -Algorithm SHA256).Hash
    $archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    Write-Host "Archive: $archive"
    Write-Host "Archive SHA-256: $archiveHash"
    Write-Host "EXE SHA-256: $exeHash"
    Write-Host "Target commit: $commit"
    Write-Host "Working tree modified: $workingTreeModified"
    Write-Host "Project: $ProjectPath"
    Write-Host 'Configuration: Release|x64'
    Write-Host "Repository: $Repository"
    Write-Host "Release tag: $Tag"
    Write-Host "Stable tag: $StableTag"
    if ($PrepareOnly) { return }

    if (-not $Title) { $Title = "SolutionInjector $Tag (Windows x64)" }
    # Передаём многострочное описание через файл, без интерпретации shell.
    $notesPath = Join-Path $outputDirectory 'ReleaseNotes.txt'
    $notes = @(
        'Windows x64 executable with app-local Visual C++ Runtime DLLs.',
        "Target commit: $commit",
        "Built from project: $ProjectPath (Release|x64).",
        "Working tree modified at build start: $workingTreeModified. Uncommitted changes are included when true.",
        "Archive SHA-256: $archiveHash",
        "EXE SHA-256: $exeHash"
    ) -join "`n"
    [IO.File]::WriteAllText($notesPath, $notes, [Text.UTF8Encoding]::new($false))
    $arguments = @('release', 'create', $Tag, $archive, '--repo', $Repository,
        '--target', $commit, '--title', $Title, '--notes-file', $notesPath)
    if ($Draft) { $arguments += '--draft' }
    # 4. Публикация, включая черновик, разрешается только после явного ввода y.
    $releaseKind = if ($Draft) { 'draft release' } else { 'release' }
    Write-Host "Ready to publish $releaseKind $Tag to $Repository."
    if (-not $Draft) { Write-Host "Stable tag $StableTag will be updated to $commit." }
    $confirmation = Read-Host 'Publish to GitHub? Type y to confirm'
    if ($confirmation -cne 'y') {
        Write-Host "Publication cancelled. Prepared archive: $archive"
        return
    }

    # Публикация не коммитит и не отправляет локальные изменения.
    InvokeChecked 'gh' $arguments | Out-Host

    # 5. После публикации перемещаем стабильный тег; ветка Last остаётся отдельной ссылкой.
    # Черновик и подготовка архива не меняют стабильный тег.
    if (-not $Draft) {
        $stableRef = @(InvokeChecked 'git' @('ls-remote', '--tags', "https://github.com/$Repository.git", "refs/tags/$StableTag"))
        if ($stableRef.Count) {
            InvokeChecked 'gh' @('api', '--method', 'PATCH', "repos/$Repository/git/refs/tags/$StableTag",
                '-f', "sha=$commit", '-F', 'force=true') | Out-Null
        } else {
            InvokeChecked 'gh' @('api', '--method', 'POST', "repos/$Repository/git/refs",
                '-f', "ref=refs/tags/$StableTag", '-f', "sha=$commit") | Out-Null
        }
        Write-Host "Stable tag $StableTag now points to $commit"
    }
}
catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}