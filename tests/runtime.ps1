param([string]$BundleDirectory, [string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BundleDirectory) { $BundleDirectory = Join-Path $repo 'native/out/windows-x86_64' }
if (-not $EvidenceDirectory) { $EvidenceDirectory = Join-Path $repo ('evidence/runtime/' + [Guid]::NewGuid().ToString()) }
New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AnimationTestWindows {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr param);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, System.Text.StringBuilder text, int max);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attribute, out uint value, int size);
    [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(IntPtr hwnd, out uint key, out byte alpha, out uint flags);
}
'@
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Find-OwnWindow([int]$ProcessId, [string]$Class) {
    $script:found = [IntPtr]::Zero
    $callback = [AnimationTestWindows+EnumProc] {
        param($Window,$Parameter)
        [uint32]$owner=0
        [void][AnimationTestWindows]::GetWindowThreadProcessId($Window,[ref]$owner)
        if ($owner -eq $ProcessId) {
            $name=[Text.StringBuilder]::new(256)
            [void][AnimationTestWindows]::GetClassName($Window,$name,256)
            if ($name.ToString() -eq $Class) { $script:found=$Window; return $false }
        }
        return $true
    }
    [void][AnimationTestWindows]::EnumWindows($callback,[IntPtr]::Zero)
    return $script:found
}
function Wait-Window([int]$ProcessId,[string]$Class) {
    $deadline=[DateTime]::UtcNow.AddSeconds(4)
    do { $h=Find-OwnWindow $ProcessId $Class; if($h -ne [IntPtr]::Zero){return $h}; Start-Sleep -Milliseconds 80 }while([DateTime]::UtcNow -lt $deadline)
    throw "Window $Class did not appear in owned process $ProcessId."
}
function Write-Record($Process,$Record) {
    $bytes=[Text.Encoding]::UTF8.GetBytes(($Record | ConvertTo-Json -Depth 30 -Compress))
    $header=[BitConverter]::GetBytes([uint32]$bytes.Length)
    $Process.StandardInput.BaseStream.Write($header,0,4)
    $Process.StandardInput.BaseStream.Write($bytes,0,$bytes.Length)
    $Process.StandardInput.BaseStream.Flush()
}
function Read-Bytes($Stream,[int]$Count) {
    $bytes=[byte[]]::new($Count); $offset=0
    while($offset -lt $Count){
        $task=$Stream.ReadAsync($bytes,$offset,$Count-$offset)
        if(-not $task.Wait(7000)){throw 'Timed out reading process-v2 output.'}
        $read=$task.Result;if($read -le 0){throw 'Unexpected process-v2 EOF.'};$offset+=$read
    }
    return ,$bytes
}
function Read-Record($Process) {
    $header=Read-Bytes $Process.StandardOutput.BaseStream 4
    $length=[BitConverter]::ToUInt32($header,0)
    Assert ($length -gt 0 -and $length -le 1048576) 'Invalid companion chunk.'
    return [Text.Encoding]::UTF8.GetString((Read-Bytes $Process.StandardOutput.BaseStream $length)) | ConvertFrom-Json
}
function Start-Session($Settings,[string]$Name) {
    $scratch=Join-Path $EvidenceDirectory $Name; New-Item -ItemType Directory -Force -Path $scratch | Out-Null
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=Join-Path $BundleDirectory 'companion.exe';$start.WorkingDirectory=$BundleDirectory
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardInput=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $start.EnvironmentVariables['MW_WINDOW_ANIMATIONS_TEST_CAPTURE']=Join-Path $scratch 'frames'
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start;[void]$process.Start()
    $stderr=$process.StandardError.ReadToEndAsync()
    Write-Record $process @{type='init';v=5;addonId='development-window-animations';artifactDigest=('sha256:'+('a'*64));version=$manifest.version;targetArchitecture='windows-x86_64';scratchDirectory=$scratch;layerSettings=@{};deviceSettings=$Settings}
    do{$record=Read-Record $process; Assert ($record.type -ne 'error') ($record | ConvertTo-Json -Compress)}while($record.type -ne 'ready')
    return @{process=$process;stderr=$stderr;scratch=$scratch}
}
function Read-Status($Session) {
    $path=Join-Path $Session.scratch 'window-animations-status.json'
    $deadline=[DateTime]::UtcNow.AddSeconds(3)
    while(-not(Test-Path -LiteralPath $path)){Assert ([DateTime]::UtcNow -lt $deadline) 'Runtime status missing.';Start-Sleep -Milliseconds 80}
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json
}
function Update-Session($Session,$Settings) {Write-Record $Session.process @{type='settings';v=5;layerSettings=@{};deviceSettings=$Settings};Start-Sleep -Milliseconds 180}
function Wait-Idle($Session) {
    $deadline=[DateTime]::UtcNow.AddSeconds(4)
    do {$status=Read-Status $Session;if($status.active -eq 0){return $status};Start-Sleep -Milliseconds 150}while([DateTime]::UtcNow -lt $deadline)
    throw 'Effects did not reach an idle runtime state.'
}
function Close-Session($Session,[switch]$Eof) {
    if(-not $Session.process.HasExited){if($Eof){$Session.process.StandardInput.Close()}else{Write-Record $Session.process @{type='shutdown';v=5}}}
    Assert ($Session.process.WaitForExit(4000)) 'Companion did not stop.'
    Assert ($Session.process.ExitCode -eq 0) 'Companion exit failed.'
    [IO.File]::WriteAllText((Join-Path $Session.scratch 'stderr.log'),$Session.stderr.Result)
}
function TemporarilyHidden([IntPtr]$Window) {[uint32]$key=0;[byte]$alpha=255;[uint32]$flags=0;$ok=[AnimationTestWindows]::GetLayeredWindowAttributes($Window,[ref]$key,[ref]$alpha,[ref]$flags);return $ok -and ($flags -band 2) -ne 0 -and $alpha -eq 0}
function Start-RawCompanion {
    $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=Join-Path $BundleDirectory 'companion.exe';$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $p=[Diagnostics.Process]::new();$p.StartInfo=$info;[void]$p.Start();return $p
}

$preview=$null;$session=$null;$observations=[Collections.Generic.List[object]]::new()
try {
    $manifest=Get-Content -Raw -Encoding UTF8 (Join-Path $repo 'manifest.json') | ConvertFrom-Json
    $defaults=@{};foreach($s in $manifest.settings){if($s.type -ne 'section'){$defaults[$s.id]=$s.default}}
    $settings=$defaults.Clone();$settings.applications='window-animations-preview.exe';$settings.respectReducedMotion=$false
    $settings.profile='custom';$settings.cacheInterval=100;$settings.cornerRadius=12
    foreach($e in @('open','close','minimize','restore')){$settings[$e+'Duration']=600}
    $session=Start-Session $settings 'transitions'
    $previewStart=[Diagnostics.ProcessStartInfo]::new()
    $previewStart.FileName=Join-Path $BundleDirectory 'window-animations-preview.exe'
    $previewStart.WorkingDirectory=$BundleDirectory
    # This is our own executable; invoking the shell from a WSL UNC path can
    # stall before launching it. Start it directly, just like the companion.
    $previewStart.UseShellExecute=$false;$previewStart.CreateNoWindow=$true
    $preview=[Diagnostics.Process]::new();$preview.StartInfo=$previewStart;[void]$preview.Start()
    $control=Wait-Window $preview.Id 'MwAnimationPreviewControl'
    foreach($mode in @('fade','slide','zoom','spring','genie','wobbly')) {
        foreach($e in @('open','close','minimize','restore')){$settings[$e+'Mode']=$mode}
        Update-Session $session $settings;Start-Sleep -Milliseconds 550
        $before=Read-Status $session
        foreach($command in @(0,1,2,3)) {
            [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]$command,[IntPtr]::Zero)
            if($command -eq 0){$canvas=Wait-Window $preview.Id 'MwAnimationPreviewCanvas'}
            Start-Sleep -Milliseconds 1000
        }
        $after=Wait-Idle $session
        foreach($e in @('open','close','minimize','restore')){Assert ($after.$e -gt $before.$e) "$mode / $e did not animate."}
        Assert ($after.frames -gt $before.frames) "$mode emitted no frames."
        Assert ($after.active -eq 0) "$mode effects did not finish."
        $framePath=Join-Path $session.scratch ('frames/open-'+$mode+'.bmp')
        $pixels=[IO.File]::ReadAllBytes($framePath);$offset=[BitConverter]::ToInt32($pixels,10)
        $blue=0
        for($p=$offset;$p -lt $pixels.Length;$p+=4){if($pixels[$p] -gt 10 -and $pixels[$p] -gt $pixels[$p+1]*1.2 -and $pixels[$p+1] -gt $pixels[$p+2]*1.2){$blue++}}
        Assert ($blue -gt ($pixels.Length-$offset)/4*0.05) "$mode opening captured an empty client surface."
        $observations.Add(@{mode=$mode;frames=($after.frames-$before.frames);open=($after.open-$before.open);close=($after.close-$before.close);minimize=($after.minimize-$before.minimize);restore=($after.restore-$before.restore);lastFrameMs=$after.lastFrameMs;dpi=[AnimationTestWindows]::GetDpiForWindow($control)})
        Write-Host "Observed four native Win32 transitions: $mode"
    }
    $before=Read-Status $session;$invalid=$settings.Clone();$invalid.openDuration=99999;Update-Session $session $invalid
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]0,[IntPtr]::Zero);Start-Sleep -Milliseconds 1000
    $after=Read-Status $session;Assert ($after.open -gt $before.open) 'Rejected settings destroyed previous configuration.'
    Assert (-not(TemporarilyHidden (Wait-Window $preview.Id 'MwAnimationPreviewCanvas'))) 'Window remained hidden after effect.'
    [void][AnimationTestWindows]::PostMessage($control,0x8014,[IntPtr]4,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    $settings.enabled=$false;Update-Session $session $settings
    $deadline=[DateTime]::UtcNow.AddMilliseconds(1200)
    do {$blocked=Read-Status $session;if(-not $blocked.enabled){break};Start-Sleep -Milliseconds 70}while([DateTime]::UtcNow -lt $deadline)
    Assert (-not $blocked.enabled) 'A blocked target window blocked companion settings.'
    $observations.Add(@{test='blocked fixture UI / capture isolation';settingsRemainedResponsive=$true})
    Start-Sleep -Milliseconds 1600
    $settings.enabled=$false;Update-Session $session $settings;Start-Sleep -Milliseconds 550;$before=Read-Status $session
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]1,[IntPtr]::Zero);Start-Sleep -Milliseconds 750
    $after=Read-Status $session;Assert ($after.minimize -eq $before.minimize) 'Disable did not suppress animation.'
    $settings.enabled=$true;$settings.minimizeEnabled=$false;Update-Session $session $settings
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]2,[IntPtr]::Zero);Start-Sleep -Milliseconds 1000
    $before=Read-Status $session;[void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]1,[IntPtr]::Zero);Start-Sleep -Milliseconds 750
    $after=Read-Status $session;Assert ($after.minimize -eq $before.minimize) 'Selective disable failed.'
    $settings.minimizeEnabled=$true;$settings.profile='fluid';Update-Session $session $settings
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]2,[IntPtr]::Zero);Start-Sleep -Milliseconds 1000
    Assert ((Read-Status $session).profile -eq 'fluid') 'Preset update failed.'
    $settings.respectReducedMotion=$true;Update-Session $session $settings;Start-Sleep -Milliseconds 550
    $before=Read-Status $session
    if($before.reducedMotion){
        [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]3,[IntPtr]::Zero);Start-Sleep -Milliseconds 300
        [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]0,[IntPtr]::Zero);Start-Sleep -Milliseconds 750
        $after=Read-Status $session
        Assert ($after.open -eq $before.open -and $after.active -eq 0) 'Windows reduced-motion preference was ignored.'
        $observations.Add(@{test='actual Windows reduced-motion preference';effectsSuppressed=$true})
    }
    $settings.respectReducedMotion=$false
    Update-Session $session $defaults;Start-Sleep -Milliseconds 550
    Assert ((Read-Status $session).profile -eq 'fluid') 'Full default reset failed.'
    Close-Session $session;$session=$null
    $settings.profile='custom';$settings.openMode='genie';$settings.restoreMode='genie';$settings.openDuration=2000;$settings.restoreDuration=2000;$settings.speed=50
    $session=Start-Session $settings 'watchdog'
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]3,[IntPtr]::Zero);Start-Sleep -Milliseconds 500
    [void][AnimationTestWindows]::SendMessage($control,0x8014,[IntPtr]0,[IntPtr]::Zero)
    $canvas=Wait-Window $preview.Id 'MwAnimationPreviewCanvas';$deadline=[DateTime]::UtcNow.AddSeconds(2)
    while(-not(TemporarilyHidden $canvas)-and[DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 30}
    Assert (TemporarilyHidden $canvas) 'Watchdog test could not establish a temporary opacity lease.'
    $session.process.Kill();[void]$session.process.WaitForExit(2000);$deadline=[DateTime]::UtcNow.AddSeconds(3)
    while((TemporarilyHidden $canvas)-and[DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 80}
    Assert (-not(TemporarilyHidden $canvas)) 'Guardian did not restore alpha after companion death.'
    $observations.Add(@{test='forced companion termination';windowRestored=$true});$session=$null
    $session=Start-Session $settings 'eof';Close-Session $session -Eof;$session=$null
    $observations.Add(@{test='stdin EOF shutdown';passed=$true})
    $session=Start-Session $defaults 'owner'
    $second=Start-RawCompanion
    try {
        Write-Record $second @{type='init';v=5;addonId='development-window-animations-second';artifactDigest=('sha256:'+('b'*64));version=$manifest.version;targetArchitecture='windows-x86_64';scratchDirectory=(Join-Path $EvidenceDirectory 'owner-second');layerSettings=@{};deviceSettings=$defaults}
        $progress=Read-Record $second;Assert ($progress.type -eq 'progress') 'Second companion skipped init/progress handshake.'
        $conflict=Read-Record $second;Assert ($conflict.code -eq 'addon_owner_conflict') 'Second layer did not report owner conflict.';Assert ($second.WaitForExit(3000)) 'Conflicting companion did not exit.'
    }finally{if(-not $second.HasExited){$second.Kill()};$second.Dispose()}
    Close-Session $session;$session=$null
    $observations.Add(@{test='single owner conflict';passed=$true})
    foreach($bad in @([byte[]](1,2,3),[BitConverter]::GetBytes([uint32]2147483649))) {
        $raw=Start-RawCompanion
        try {$raw.StandardInput.BaseStream.Write($bad,0,$bad.Length);$raw.StandardInput.Close();$protocolError=Read-Record $raw;Assert ($protocolError.code -eq 'invalid_wire') 'Malformed chunk was not rejected.';Assert ($raw.WaitForExit(3000)) 'Malformed session did not exit.';Assert ($raw.ExitCode -eq 1) 'Malformed wire exited successfully.'}finally{if(-not $raw.HasExited){$raw.Kill()};$raw.Dispose()}
    }
    $observations.Add(@{test='truncated / out-of-order wire chunks';passed=$true})
    $report=@{date=[DateTime]::UtcNow.ToString('yyyy-MM-dd');windows=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber;settingsDefinitions=$manifest.settings.Count;host='owned Win32 preview process';observations=$observations;limitations=@('Accelerated/translucent applications, secondary monitors, high DPI and real MyWallpaper sidebar not exercised by this fixture.')}
    [IO.File]::WriteAllText((Join-Path $EvidenceDirectory 'runtime-results.json'),($report | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    Write-Host "Runtime observations saved: $EvidenceDirectory"
} finally {
    if($session -and -not $session.process.HasExited){try{Close-Session $session}catch{$session.process.Kill()}}
    if($preview -and -not $preview.HasExited){$control=Find-OwnWindow $preview.Id 'MwAnimationPreviewControl';if($control -ne [IntPtr]::Zero){[void][AnimationTestWindows]::PostMessage($control,0x10,[IntPtr]::Zero,[IntPtr]::Zero)};if(-not$preview.WaitForExit(2000)){$preview.Kill()}}
}
