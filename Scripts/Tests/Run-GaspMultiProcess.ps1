[CmdletBinding()]
param(
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Label = 'diagnostic',
    [ValidateSet(15, 30, 60, 120)][int[]]$Fps = @(15, 30, 60, 120),
    [string]$ProjectRoot = 'D:\Repos\SurvivalRpg',
    [string]$Editor = 'D:\Programme\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [ValidateRange(1024, 65000)][int]$BasePort = 17877,
    [ValidateRange(0, 1000)][int]$PacketLag = 60,
    [ValidateRange(0, 1000)][int]$PacketLagVariance = 10,
    [ValidateRange(0, 100)][int]$PacketLoss = 10,
    [switch]$NoHitch,
    [switch]$Offscreen,
    # Transient render-only diagnostic settings; measured frame deltas still define coverage.
    [switch]$ReducedRendering,
    [switch]$CaptureScreenshots
)

# Launch only after the central build is complete. This creates independent editor processes;
# each DiagnosticRole automation owns exactly one PIE world and uses the IP net driver.
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$runName = '{0}-{1}-{2}' -f $Label, [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'), [Guid]::NewGuid().ToString('N').Substring(0,8)
$runRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "Saved\Reviews\GaspMultiProcess\$runName"))
$savedRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved')) + [IO.Path]::DirectorySeparatorChar
if (-not $runRoot.StartsWith($savedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Output escapes project Saved.' }
$null = New-Item -ItemType Directory -Path $runRoot -Force

function Quote-NativeArgument([string]$Value) {
    if ($Value.Contains('"')) { throw 'Embedded quotes are not accepted in harness paths.' }
    '"' + $Value + '"'
}

$reducedRenderingCommands = @()
if ($ReducedRendering) {
    $reducedRenderingCommands = @(
        'sg.ShadowQuality 0',
        'sg.GlobalIlluminationQuality 0',
        'sg.ReflectionQuality 0',
        'sg.PostProcessQuality 0',
        'r.ScreenPercentage 50'
    )
}
# UE ParseExecCommands separates deferred commands with commas. Keep the Automation command's
# own semicolon-delimited queue intact, and apply rendering settings before starting that queue.
$execCommands = @($reducedRenderingCommands) + @('Automation RunTests SurvivalRpg.Network.GaspMultiProcess.DiagnosticRole; Quit')

function Start-GaspRole([string]$Role, [string]$CaseDirectory, [int]$Limit, [int]$Port) {
    $roleDirectory = Join-Path $CaseDirectory $Role
    $null = New-Item -ItemType Directory -Path $roleDirectory -Force
    $arguments = @(
        (Join-Path $ProjectRoot 'SurvivalRpg.uproject'),
        '-unattended', '-nop4', '-nosteam', '-nosplash', '-nosound', '-windowed', '-ResX=640', '-ResY=480',
        '-stdout', '-FullStdOutLogOutput', '-NoLogTimes', '-ddc=InstalledNoZenLocalFallback',
        '-ini:Engine:[/Script/Engine.AutomationTestSettings]:DefaultInteractiveFramerate=5',
        "-UserDir=$roleDirectory\User", "-ShaderWorkingDir=$roleDirectory\Shaders",
        "-LocalDataCachePath=$ProjectRoot\Intermediate\GaspNetworkDDC",
        "-GaspProcessRole=$Role", "-GaspTraceDir=$CaseDirectory", "-GaspTraceFPS=$Limit", "-GaspTracePort=$Port",
        "-GaspTraceLag=$PacketLag", "-GaspTraceLagVariance=$PacketLagVariance", "-GaspTraceLoss=$PacketLoss",
        ('-ExecCmds=' + ($execCommands -join ', ')),
        '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$roleDirectory\Automation", "-abslog=$roleDirectory\Editor.log"
    )
    if ($Offscreen) { $arguments += '-RenderOffscreen' }
    if ($CaptureScreenshots) { $arguments += '-GaspCaptureScreenshots' }
    if ($NoHitch) { $arguments += '-GaspTraceNoHitch' }
    $argumentLine = ($arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    Start-Process -FilePath $Editor -ArgumentList $argumentLine -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru
}

function Wait-GaspMarker([string]$Path, [System.Collections.Generic.List[System.Diagnostics.Process]]$Processes, [int]$Seconds = 180) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while (-not (Test-Path -LiteralPath $Path)) {
        foreach ($process in $Processes) {
            $process.Refresh()
            if ($process.HasExited) { throw "Process $($process.Id) exited before marker $Path. Inspect its Editor.log." }
        }
        if ([DateTime]::UtcNow -ge $deadline) { throw "Timed out awaiting $Path" }
        Start-Sleep -Milliseconds 500
    }
}

$metadata = [ordered]@{
    label = $Label; project = $ProjectRoot; editor = $Editor; started_utc = [DateTime]::UtcNow.ToString('o')
    topology = 'three OS processes: listen host, autonomous owner, late observer'
    packet_lag_ms = $PacketLag; packet_lag_variance_ms = $PacketLagVariance; packet_loss_percent = $PacketLoss; injected_hitch = -not [bool]$NoHitch
    frame_limits = $Fps; offscreen = [bool]$Offscreen; capture_screenshots = [bool]$CaptureScreenshots; output = $runRoot
    editor_startup_min_fps = 5
    reduced_rendering = [ordered]@{ enabled = [bool]$ReducedRendering; commands = @($reducedRenderingCommands) }
    scope = 'editor-process IP transport and synchronized diagnostic capture; not packaged/Steam acceptance or an uninstrumented performance benchmark'
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runRoot 'run.json') -Encoding utf8

for ($index = 0; $index -lt $Fps.Count; $index++) {
    $limit = $Fps[$index]
    $caseDirectory = Join-Path $runRoot "fps-$limit"
    $null = New-Item -ItemType Directory -Path $caseDirectory -Force
    $processes = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
    try {
        $processes.Add((Start-GaspRole 'server' $caseDirectory $limit ($BasePort + $index)))
        Wait-GaspMarker (Join-Path $caseDirectory 'server.ready') $processes
        $processes.Add((Start-GaspRole 'owner' $caseDirectory $limit ($BasePort + $index)))
        Wait-GaspMarker (Join-Path $caseDirectory 'late.request') $processes
        $processes.Add((Start-GaspRole 'late' $caseDirectory $limit ($BasePort + $index)))
        # Processes may exit after their own done marker; completion is verified by all three markers, not process liveness.
        $deadline = [DateTime]::UtcNow.AddSeconds(240)
        do {
            foreach ($role in @('server', 'owner', 'late')) {
                $donePath = Join-Path $caseDirectory "$role.done"
                if ((Test-Path -LiteralPath $donePath) -and (Get-Content -LiteralPath $donePath -Raw).Trim() -ne 'success') {
                    throw "FPS $limit role $role reported failure; inspect $caseDirectory\$role\Editor.log"
                }
            }
            $missing = @('server', 'owner', 'late') | Where-Object { -not (Test-Path -LiteralPath (Join-Path $caseDirectory "$_.done")) }
            if ($missing.Count -eq 0) { break }
            for ($processIndex = 0; $processIndex -lt $processes.Count; $processIndex++) {
                $process = $processes[$processIndex]
                $process.Refresh()
                if ($process.HasExited) {
                    $processRole = @('server', 'owner', 'late')[$processIndex]
                    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $caseDirectory "$processRole.done"))) {
                        throw "Role $processRole process $($process.Id) exited with code $($process.ExitCode) before successful completion."
                    }
                }
            }
            if ([DateTime]::UtcNow -ge $deadline) { throw "Completion timed out for roles: $missing" }
            Start-Sleep -Milliseconds 500
        } while ($true)
        foreach ($role in @('server', 'owner', 'late')) {
            $result = (Get-Content -LiteralPath (Join-Path $caseDirectory "$role.done") -Raw).Trim()
            if ($result -ne 'success') { throw "FPS $limit role $role failed; inspect $caseDirectory\$role\Editor.log" }
        }
        Write-Output "Captured FPS $limit at $caseDirectory"
    }
    finally {
        # Only children launched and retained by this invocation are terminated; never enumerate other editor processes.
        foreach ($process in $processes) {
            $process.Refresh()
            if (-not $process.HasExited) {
                if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
            }
            $process.Dispose()
        }
    }
}
Write-Output "All requested captures complete: $runRoot"
