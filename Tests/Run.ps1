param([Parameter(Mandatory)][string]$Executable)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$workspace = Join-Path ([IO.Path]::GetTempPath()) ('SolutionInjector-model-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $workspace | Out-Null
Copy-Item -Path (Join-Path $PSScriptRoot 'Fixtures/*') -Destination $workspace
$checks = 0

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}

function Hash([string]$Path) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToBase64String($algorithm.ComputeHash([IO.File]::ReadAllBytes($Path))) }
    finally { $algorithm.Dispose() }
}

function Run([string]$Name, [int]$ExitCode, [string[]]$Options = @()) {
    $output = & $Executable prune (Join-Path $workspace $Name) --scope Managed --json @Options
    Assert ($LASTEXITCODE -eq $ExitCode) "Unexpected exit code for $Name"
    return ($output | ConvertFrom-Json)
}

foreach ($name in @('one.sln', 'two.sln', 'sln-dependency.sln', 'shared-metadata.sln')) {
    $path = Join-Path $workspace $name
    $before = Hash $path
    $result = Run $name 0
    Assert (@($result.removedProjects).Count -eq 1) 'Used shared project was selected for removal'
    Assert ((Hash $path) -eq $before) 'Preview changed solution'
}
foreach ($name in @('transitive.sln', 'cycle.sln')) {
    $result = Run $name 0
    Assert (@($result.removedProjects).Count -eq 0) 'Transitive dependency or cycle was removed'
}
$result = Run 'last.sln' 0 @('--root', 'Shared')
Assert (@($result.removedProjects).Count -eq 1) 'Explicit root was not retained'
$result = Run 'last.sln' 0 @('--apply')
Assert ($result.applied -and @($result.removedProjects).Count -eq 2 -and @($result.removedFolders).Count -eq 1) 'Last disconnect did not clean subtree'
$content = [IO.File]::ReadAllText((Join-Path $workspace 'last.sln'))
Assert ($content.Contains('VisualStudioVersion = 18.0.12345.0')) 'Original solution header was lost'
Assert ($content.Contains('Opaque = preserve this')) 'Unknown global section was lost'
Assert (-not $content.Contains('33333333-3333-3333-3333-333333333333')) 'Removed configuration or nesting survived'
Assert (Test-Path -LiteralPath (Join-Path $workspace 'Shared.vcxitems')) 'Source file was deleted'
Assert (@(Get-ChildItem -LiteralPath $workspace -Filter 'last.sln.SolutionInjector.*.bak').Count -eq 1) 'Backup missing'
foreach ($name in @('unknown.sln', 'missing.sln')) {
    $path = Join-Path $workspace $name
    $before = Hash $path
    $result = Run $name 2 @('--apply')
    Assert ($result.blocked -and -not $result.applied) 'Unresolved dependency did not block cleanup'
    Assert ((Hash $path) -eq $before) 'Blocked cleanup changed solution'
}
$result = Run 'two.sln' 0 @('--apply')
$path = Join-Path $workspace 'two.sln'
$before = Hash $path
$result = Run 'two.sln' 0 @('--apply')
Assert (@($result.removedProjects).Count -eq 0 -and (Hash $path) -eq $before) 'Repeated cleanup changed solution'
foreach ($arguments in @(
    @('prune', (Join-Path $workspace 'one.sln')),
    @('prune', (Join-Path $workspace 'one.sln'), '--scope'),
    @('prune', (Join-Path $workspace 'one.sln'), '--scope', 'Managed', '--root', '--apply'),
    @('prune', (Join-Path $workspace 'one.sln'), '--scope', 'Managed', '--unknown'),
    @('analyze', (Join-Path $workspace 'one.sln'), '--scope', 'Managed', '--apply'),
    @('prune', (Join-Path $workspace 'one.sln'), 'extra', '--scope', 'Managed')
)) {
    & $Executable @arguments | Out-Null
    Assert ($LASTEXITCODE -eq 1) 'Invalid arguments were accepted'
}
$result = Run 'one.sln' 0 @('--root', 'Shared', '--root', 'Unused')
Assert (@($result.removedProjects).Count -eq 0) 'Repeated --root lost a value'
Write-Host "Passed $checks checks using the existing SolutionStructure. Fixtures: $workspace"