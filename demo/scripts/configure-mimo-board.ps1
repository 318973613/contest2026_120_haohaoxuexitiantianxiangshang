[CmdletBinding()]
param(
    [string]$PortName = "COM15",
    [int]$BaudRate = 1500000
)

$ErrorActionPreference = "Stop"

$BaseUrl = "https://token-plan-cn.xiaomimimo.com/v1"
$HostName = "token-plan-cn.xiaomimimo.com"
$Model = "mimo-v2.5-pro"
$serial = $null
$keyPtr = [IntPtr]::Zero
$apiKey = $null

function Read-SerialWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,
        [int]$TimeoutMs = 3000
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    $text = New-Object System.Text.StringBuilder
    while ([DateTime]::UtcNow -lt $deadline) {
        $chunk = $Serial.ReadExisting()
        if ($chunk.Length -gt 0) {
            [void]$text.Append($chunk)
        }
        Start-Sleep -Milliseconds 80
    }
    return $text.ToString()
}

function Send-Command {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Serial,
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [int]$TimeoutMs = 3000
    )

    $Serial.Write($Command + "`r`n")
    return Read-SerialWindow -Serial $Serial -TimeoutMs $TimeoutMs
}

try {
    $serial = [System.IO.Ports.SerialPort]::new(
        $PortName,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    $serial.DtrEnable = $true
    $serial.RtsEnable = $false
    $serial.ReadTimeout = 200
    $serial.WriteTimeout = 2000
    $serial.NewLine = "`r`n"
    $serial.Open()

    $serial.DiscardInBuffer()
    $serial.Write("`r`n")
    $prompt = Read-SerialWindow -Serial $serial -TimeoutMs 1500

    if ($prompt -notmatch "vela>") {
        if ($prompt -match "nsh>") {
            $prompt = Send-Command -Serial $serial -Command "ai_agent" -TimeoutMs 5000
        }
    }

    if ($prompt -notmatch "vela>") {
        throw "The board did not return a vela> prompt. Reset it, wait for boot, and close other serial terminals before retrying."
    }

    $secureKey = Read-Host "Enter a NEW Xiaomi MiMo API key (input is hidden)" -AsSecureString
    $keyPtr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureKey)
    $apiKey = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($keyPtr)

    if ([string]::IsNullOrWhiteSpace($apiKey)) {
        throw "No key entered; configuration cancelled."
    }
    if ($apiKey -match "\s") {
        throw "The key must not contain spaces; configuration cancelled."
    }
    if ($apiKey.Length -le 6) {
        throw "The key is too short to verify safely as masked output."
    }

    $maskedPrefix = $apiKey.Substring(0, 4) + "****"
    $serial.DiscardInBuffer()
    $serial.Write("set_llm $BaseUrl $Model ")
    $serial.Write($apiKey + "`r`n")
    $setOutput = Read-SerialWindow -Serial $serial -TimeoutMs 5000

    if ($setOutput -notmatch "API key saved") {
        throw "The board did not confirm that the MiMo key was saved."
    }

    # Never print the set_llm response because the serial console may echo the key.
    $serial.DiscardInBuffer()
    $configOutput = Send-Command -Serial $serial -Command "config_show" -TimeoutMs 3000
    if ($configOutput.Contains($apiKey)) {
        throw "config_show exposed the unmasked key; output was discarded."
    }
    if ($configOutput -notmatch [regex]::Escape($maskedPrefix) -or
        $configOutput -notmatch [regex]::Escape($HostName) -or
        $configOutput -notmatch [regex]::Escape($Model)) {
        throw "config_show did not return the expected masked MiMo configuration."
    }

    $serial.DiscardInBuffer()
    $routerOutput = Send-Command -Serial $serial -Command "router_status" -TimeoutMs 3000
    if ($routerOutput.Contains($apiKey)) {
        throw "router_status exposed the unmasked key; output was discarded."
    }
    if ($routerOutput -notmatch [regex]::Escape($HostName) -or
        $routerOutput -notmatch [regex]::Escape($Model)) {
        throw "router_status did not report the expected MiMo backend."
    }

    Write-Host "MiMo configuration saved successfully."
    Write-Host "Base URL: $BaseUrl"
    Write-Host "Model: $Model"
    Write-Host "Verification: config_show masked the key and router_status found MiMo."
    Write-Host "The full API key was not written to a file or printed by this script."
}
finally {
    $prompt = $null
    $setOutput = $null
    $configOutput = $null
    $routerOutput = $null
    $apiKey = $null
    if ($keyPtr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($keyPtr)
    }
    if ($null -ne $serial) {
        if ($serial.IsOpen) {
            $serial.DiscardInBuffer()
            $serial.Close()
        }
        $serial.Dispose()
    }
}
