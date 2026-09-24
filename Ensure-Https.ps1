Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-PrimaryLanIPv4 {
    try {
        $routes = Get-NetRoute -AddressFamily IPv4 -DestinationPrefix "0.0.0.0/0" -ErrorAction Stop |
            Where-Object { $_.State -eq "Alive" } |
            Sort-Object RouteMetric
        foreach ($route in $routes) {
            $candidate = Get-NetIPAddress -AddressFamily IPv4 -InterfaceIndex $route.InterfaceIndex -ErrorAction SilentlyContinue |
                Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" } |
                Select-Object -First 1 -ExpandProperty IPAddress
            if ($candidate) { return $candidate }
        }
    } catch {}

    $fallback = [System.Net.Dns]::GetHostAddresses([System.Net.Dns]::GetHostName()) |
        Where-Object {
            $_.AddressFamily -eq [System.Net.Sockets.AddressFamily]::InterNetwork -and
            $_.IPAddressToString -notlike "127.*" -and
            $_.IPAddressToString -notlike "169.254.*"
        } | Select-Object -First 1
    if (!$fallback) { throw "Could not determine a LAN IPv4 address for PencilBridge HTTPS." }
    return $fallback.IPAddressToString
}

$ip = Get-PrimaryLanIPv4
$tlsDir = Join-Path $PSScriptRoot "Saved\PencilBridge\TLS"
New-Item -ItemType Directory -Force -Path $tlsDir | Out-Null

$rootSubject = "CN=PencilBridge Local CA"
$serverSubject = "CN=PencilBridge Local Server"
$now = Get-Date

$root = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $rootSubject -and $_.HasPrivateKey -and $_.NotAfter -gt $now.AddDays(30) } |
    Sort-Object NotAfter -Descending | Select-Object -First 1

if (!$root) {
    Write-Host "Creating PencilBridge local certificate authority..."
    $rootArgs = @{
        Type = "Custom"
        Subject = $rootSubject
        CertStoreLocation = "Cert:\CurrentUser\My"
        KeyAlgorithm = "RSA"
        KeyLength = 2048
        HashAlgorithm = "SHA256"
        KeyExportPolicy = "Exportable"
        KeyUsage = @("CertSign", "CRLSign", "DigitalSignature")
        NotAfter = $now.AddYears(10)
        TextExtension = @(
            "2.5.29.19={critical}{text}ca=1&pathlength=1"
        )
    }
    $root = New-SelfSignedCertificate @rootArgs
}

$caCer = Join-Path $tlsDir "PencilBridge-CA.cer"
Export-Certificate -Cert $root -FilePath $caCer -Type CERT -Force | Out-Null

$trustedRoot = Get-ChildItem Cert:\CurrentUser\Root |
    Where-Object { $_.Thumbprint -eq $root.Thumbprint } | Select-Object -First 1
if (!$trustedRoot) {
    Import-Certificate -FilePath $caCer -CertStoreLocation "Cert:\CurrentUser\Root" | Out-Null
}

Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $serverSubject } |
    Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "Creating PencilBridge HTTPS certificate for $ip..."
$serverArgs = @{
    Type = "Custom"
    Subject = $serverSubject
    Signer = $root
    CertStoreLocation = "Cert:\CurrentUser\My"
    KeyAlgorithm = "RSA"
    KeyLength = 2048
    HashAlgorithm = "SHA256"
    KeyExportPolicy = "Exportable"
    KeyUsage = @("DigitalSignature", "KeyEncipherment")
    NotAfter = $now.AddYears(2)
    TextExtension = @(
        "2.5.29.17={text}IPAddress=$ip&DNS=$env:COMPUTERNAME&DNS=localhost",
        "2.5.29.37={text}1.3.6.1.5.5.7.3.1"
    )
}
$server = New-SelfSignedCertificate @serverArgs

[IO.File]::WriteAllText((Join-Path $tlsDir "server-ip.txt"), $ip, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $tlsDir "server-thumbprint.txt"), $server.Thumbprint, [Text.UTF8Encoding]::new($false))

$certBase64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($caCer))
$profile = @"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>PayloadContent</key>
    <array>
        <dict>
            <key>PayloadCertificateFileName</key><string>PencilBridge-CA.cer</string>
            <key>PayloadContent</key><data>$certBase64</data>
            <key>PayloadDescription</key><string>Trusts the local PencilBridge HTTPS server.</string>
            <key>PayloadDisplayName</key><string>PencilBridge Local CA</string>
            <key>PayloadIdentifier</key><string>com.pencilbridge.local.ca</string>
            <key>PayloadType</key><string>com.apple.security.root</string>
            <key>PayloadUUID</key><string>23FDD79A-1248-4F26-9DCB-A0B7D787FBE1</string>
            <key>PayloadVersion</key><integer>1</integer>
        </dict>
    </array>
    <key>PayloadDescription</key><string>One-time PencilBridge HTTPS trust profile.</string>
    <key>PayloadDisplayName</key><string>PencilBridge Local HTTPS</string>
    <key>PayloadIdentifier</key><string>com.pencilbridge.local.profile</string>
    <key>PayloadOrganization</key><string>PencilBridge</string>
    <key>PayloadType</key><string>Configuration</string>
    <key>PayloadUUID</key><string>6D27A2D1-6725-4E29-BD98-5B2EC1B83F2C</string>
    <key>PayloadVersion</key><integer>1</integer>
</dict>
</plist>
"@

[IO.File]::WriteAllText((Join-Path $tlsDir "PencilBridge-CA.mobileconfig"), $profile, [Text.UTF8Encoding]::new($false))

Write-Host ""
Write-Host "PencilBridge HTTPS ready:"
Write-Host ("  Secure app: https://{0}:8765" -f $ip)
Write-Host ("  First-time iPad setup: http://{0}:8764" -f $ip)
Write-Host ""
