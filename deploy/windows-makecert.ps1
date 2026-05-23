param (
    [Parameter(Mandatory, HelpMessage="Enter certificate password")]
    [string]$Password,

    [switch]$Import
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path "deploy")) {
    Write-Error "Must be run from the project root"
    exit 1
}

$DEPLOY_DIR = Join-Path $PWD "deploy"

$cert = New-SelfSignedCertificate -Subject "CN=Monidroid Self-Signed Certificate" -Type CodeSigningCert -CertStoreLocation "Cert:\CurrentUser\My\"
$certPassword = ConvertTo-SecureString -String $Password -Force -AsPlainText
Export-PfxCertificate -Cert $cert -Password $certPassword -FilePath "$DEPLOY_DIR\MonidroidPfx.pfx"
Export-Certificate -Cert $cert -FilePath "$DEPLOY_DIR\MonidroidCert.cer"

#makecert -r -sv "$DEPLOY_DIR\md.pvk" -n CN="Monidroid" "$DEPLOY_DIR\MonidroidCert.cer"
#cert2spc "$DEPLOY_DIR\MonidroidCert.cer" "$DEPLOY_DIR\md.spc"
#pvk2pfx -pvk "$DEPLOY_DIR\md.pvk" -pi $Password -spc "$DEPLOY_DIR\md.spc" -pfx "$DEPLOY_DIR\MonidroidPfx.pfx" -po $Password
#Remove-Item "$DEPLOY_DIR\md.pvk", "$DEPLOY_DIR\md.spc"

if ($Import) {
    Write-Output "Installing a certificate..."

    Import-Certificate -CertStoreLocation Cert:\LocalMachine\AuthRoot -FilePath "$DEPLOY_DIR\MonidroidCert.cer"
    Import-Certificate -CertStoreLocation Cert:\LocalMachine\TrustedPublisher -FilePath "$DEPLOY_DIR\MonidroidCert.cer"
}