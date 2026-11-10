param([Parameter(Mandatory)][string]$Executable)

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$workspace = Join-Path ([IO.Path]::GetTempPath()) ('SolutionInjector-sync-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($workspace)
$type = '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}'
$hostGuid = '{11111111-1111-1111-1111-111111111111}'
$ownedGuid = '{22222222-2222-2222-2222-222222222222}'
$sharedGuid = '{33333333-3333-3333-3333-333333333333}'
$folderGuid = '{44444444-4444-4444-4444-444444444444}'
$target = Join-Path $workspace 'target.sln'
$source = Join-Path $workspace 'desired.sln'
$checks = 0
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message }; $script:checks++ }
function WriteFile([string]$Path, [string]$Text) { [IO.File]::WriteAllText($Path, $Text.TrimEnd([char[]]"`r`n"), [Text.UTF8Encoding]::new($false)) }
function Project([string]$Name, [string]$Path, [string]$Guid) { return "Project(`"$type`") = `"$Name`", `"$Path`", `"$Guid`"`nEndProject" }
function Solution($Projects, $Mappings, $Shared = @(), $Nested = @(), [string]$Configuration = 'Debug|x64') {
    return (@('Microsoft Visual Studio Solution File, Format Version 12.00', '# Visual Studio Version 18', 'VisualStudioVersion = 18.0.12345.0') + $Projects + @('Global', "`tGlobalSection(SolutionConfigurationPlatforms) = preSolution", "`t`t$Configuration = $Configuration", "`tEndGlobalSection", "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution") + $Mappings + @("`tEndGlobalSection", "`tGlobalSection(SharedMSBuildProjectFiles) = preSolution") + $Shared + @("`tEndGlobalSection", "`tGlobalSection(NestedProjects) = preSolution") + $Nested + @("`tEndGlobalSection", "`tGlobalSection(UnknownExtension) = preSolution", "`t`tOpaque = preserve this", "`tEndGlobalSection", 'EndGlobal')) -join "`n"
}
function Sync([int]$Expected = 0, [string[]]$Options = @(), [string]$Owner = 'ProjectTools') {
    $json = & $Executable sync $source $target --owner $Owner --json @Options
    Assert ($LASTEXITCODE -eq $Expected) "Unexpected sync exit code: $json"
    return $json | ConvertFrom-Json
}
try {
    foreach ($file in @('Host.vcxproj', 'Owned.vcxproj', 'Renamed.vcxproj', 'Shared.vcxitems')) { WriteFile (Join-Path $workspace $file) '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003" />' }
    $hostProject = Project 'Host' 'Host.vcxproj' $hostGuid
    $ownedProject = (Project 'Owned' 'Owned.vcxproj' $ownedGuid).Replace('EndProject', "`tProjectSection(CustomOwnedSection) = preProject`n`t`tCustom = keep`n`tEndProjectSection`nEndProject")
    WriteFile $target (Solution @($hostProject, $ownedProject) @("`t`t$hostGuid.Debug|x64.ActiveCfg = Debug|x64", "`t`t$ownedGuid.Debug|x64.ActiveCfg = Debug|x64"))
    $folder = "Project(`"{2150E333-8FDC-42A3-9474-1A3956D46DE8}`") = `"Generated`", `"Generated`", `"$folderGuid`"`nEndProject"
    WriteFile $source (Solution @($folder, (Project 'Owned' 'Owned.vcxproj' $ownedGuid), (Project 'Shared' 'Shared.vcxitems' $sharedGuid)) @("`t`t$ownedGuid.Debug|x64.ActiveCfg = Release|Win32", "`t`t$ownedGuid.Debug|x64.Build.0 = Release|Win32") @("`t`tShared.vcxitems*$ownedGuid*SharedItemsImports = 4") @("`t`t$ownedGuid = $folderGuid", "`t`t$sharedGuid = $folderGuid"))
    $before = [IO.File]::ReadAllText($target)
    $report = Sync 2
    Assert ($report.blocked -and [IO.File]::ReadAllText($target) -ceq $before) 'Implicit adoption changed the solution'
    $report = Sync 0 @('--adopt', $ownedGuid)
    Assert ($report.changed -and -not $report.applied -and [IO.File]::ReadAllText($target) -ceq $before) 'Preview changed target'
    $report = Sync 0 @('--adopt', $ownedGuid, '--apply')
    $text = [IO.File]::ReadAllText($target)
    Assert ($report.applied -and $text.Contains('SolutionInjectorProjectOwnership')) 'Ownership was not persisted'
    Assert ($text.Contains('CustomOwnedSection') -and $text.Contains('Opaque = preserve this') -and $text.Contains('18.0.12345.0')) 'Custom data or header was lost'
    Assert ($text.Contains("$ownedGuid.Debug|x64.ActiveCfg = Release|Win32") -and $text.Contains("$hostGuid.Debug|x64.ActiveCfg = Debug|x64")) 'Mappings were not synchronized or unmanaged mapping was changed'
    Assert ($text.Contains("$ownedGuid = $folderGuid")) 'Managed project was not moved into its folder'
    $before = [IO.File]::ReadAllText($target)
    $stamp = (Get-Item -LiteralPath $target).LastWriteTimeUtc
    $report = Sync 0 @('--apply')
    Assert (-not $report.changed -and [IO.File]::ReadAllText($target) -ceq $before -and (Get-Item -LiteralPath $target).LastWriteTimeUtc -eq $stamp) 'Repeated sync rewrote target'
    $report = Sync 2 @('--adopt', $ownedGuid, '--apply') 'AnotherOwner'
    Assert ($report.blocked -and [IO.File]::ReadAllText($target) -ceq $before) 'Another owner took control'

    $desiredBefore = [IO.File]::ReadAllText($source)
    WriteFile $source (Solution @((Project 'Collision' 'Host.vcxproj' $sharedGuid)) @())
    $report = Sync 2 @('--apply')
    Assert ($report.blocked -and [IO.File]::ReadAllText($target) -ceq $before) 'Path collision modified the target'
    WriteFile $source $desiredBefore
    WriteFile $target ($before.Replace('"Owned", "Owned.vcxproj"', '"Owned", "Host.vcxproj"'))
    $drift = [IO.File]::ReadAllText($target)
    $report = Sync 2 @('--apply')
    Assert ($report.blocked -and [IO.File]::ReadAllText($target) -ceq $drift) 'External identity drift was overwritten'
    WriteFile $target $before

    # Проверяем переименование, новую конфигурацию и удаление только управляемого Shared.
    WriteFile $source (Solution @($folder, (Project 'Renamed' 'Renamed.vcxproj' $ownedGuid)) @("`t`t$ownedGuid.Release|ARM64.ActiveCfg = Release|ARM64", "`t`t$ownedGuid.Release|ARM64.Build.0 = Release|ARM64") @() @("`t`t$ownedGuid = $folderGuid") 'Release|ARM64')
    $report = Sync 0 @('--apply')
    $text = [IO.File]::ReadAllText($target)
    Assert (-not $text.Contains($sharedGuid) -and -not $text.Contains('SharedItemsImports')) 'Stale Shared references survived'
    Assert ($text.Contains('Renamed.vcxproj') -and -not $text.Contains("$ownedGuid.Debug|x64")) 'Project identity or mappings were not updated'
    Assert ($text.Contains('Release|ARM64 = Release|ARM64') -and $text.Contains('Debug|x64 = Debug|x64')) 'Solution configurations were not merged'
    Assert (Test-Path -LiteralPath (Join-Path $workspace 'Shared.vcxitems')) 'Sync deleted a project file'
    $report = Sync 0 @('--apply')
    Assert (-not $report.changed) 'Second updated sync was not stable'

    # Посторонняя XML-ссылка блокирует удаление управляемого проекта.
    WriteFile (Join-Path $workspace 'Host.vcxproj') '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup><ProjectReference Include="Renamed.vcxproj" /></ItemGroup></Project>'
    WriteFile $source (Solution @() @())
    $before = [IO.File]::ReadAllText($target)
    $report = Sync 2 @('--apply')
    Assert ($report.blocked -and [IO.File]::ReadAllText($target) -ceq $before) 'External reference did not block removal'
    WriteFile (Join-Path $workspace 'Host.vcxproj') '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003" />'
    $report = Sync 0 @('--apply')
    Assert (-not [IO.File]::ReadAllText($target).Contains($ownedGuid) -and [IO.File]::ReadAllText($target).Contains($hostGuid)) 'Owned removal affected unmanaged host'
    Assert (@(Get-ChildItem -LiteralPath $workspace -Filter 'target.sln.SolutionInjector.*.bak').Count -ge 3) 'Sync backup was not created'
    # Старые команды вставки и нормализации работают без sync и его параметров.
    $legacy = Join-Path $workspace 'legacy.sln'
    WriteFile $legacy (Solution @($hostProject) @("`t`t$hostGuid.Debug|x64.ActiveCfg = Debug|x64"))
    WriteFile $source (Solution @($folder, (Project 'Shared' 'Shared.vcxitems' $sharedGuid)) @() @() @("`t`t$sharedGuid = $folderGuid"))
    & $Executable $source $legacy --rootName UniversalDevKit -f Generated
    Assert ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($legacy).Contains('UniversalDevKit') -and [IO.File]::ReadAllText($legacy).Contains($sharedGuid)) 'Legacy Connect-style insertion failed'
    & $Executable $source $legacy --normalize
    Assert ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText($legacy).Contains('Opaque = preserve this')) 'Legacy normalization lost unknown data'

    # BOM и CRLF сохраняются; повторный sync не создаёт новую резервную копию.
    WriteFile $source (Solution @() @())
    $crlf = [IO.File]::ReadAllText($target).Replace("`r`n", "`n").Replace("`n", "`r`n")
    [IO.File]::WriteAllText($target, $crlf, [Text.UTF8Encoding]::new($true))
    $report = Sync 0 @('--apply')
    $bytes = [IO.File]::ReadAllBytes($target)
    Assert ($bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191 -and [IO.File]::ReadAllText($target).Contains("`r`n")) 'BOM/CRLF were not retained'
    Write-Host "PASS: sync ($checks checks)."
} finally {
    $resolved = [IO.Path]::GetFullPath($workspace)
    $root = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolved) -notlike 'SolutionInjector-sync-*') { throw 'Unsafe test directory.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}