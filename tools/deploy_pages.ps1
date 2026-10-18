# Deploy updated binaries and web flasher to gh-pages branch
$rootDir = "d:\tool\RemoteMapper-ESP32"
$flasherDir = Join-Path $rootDir "RemoteMapper-Flasher"
$webFlasherDir = Join-Path $rootDir "web-flasher"
$ghPagesTempDir = Join-Path $env:TEMP ("remotemapper-ghpages-" + [System.Guid]::NewGuid().ToString().Substring(0,8))

try {
    New-Item -ItemType Directory -Path $ghPagesTempDir -Force | Out-Null
    Copy-Item "$webFlasherDir\index.html" "$ghPagesTempDir\" -Force
    if (Test-Path -LiteralPath "$webFlasherDir\flash.html") { Copy-Item "$webFlasherDir\flash.html" "$ghPagesTempDir\" -Force }
    if (Test-Path -LiteralPath "$webFlasherDir\i18n.js") { Copy-Item "$webFlasherDir\i18n.js" "$ghPagesTempDir\" -Force }
    Copy-Item "$webFlasherDir\esptool-bundle.js" "$ghPagesTempDir\" -Force
    if (Test-Path -LiteralPath "$webFlasherDir\robots.txt") { Copy-Item "$webFlasherDir\robots.txt" "$ghPagesTempDir\" -Force }
    if (Test-Path -LiteralPath "$webFlasherDir\sitemap.xml") { Copy-Item "$webFlasherDir\sitemap.xml" "$ghPagesTempDir\" -Force }

    $firmwareDestDir = Join-Path $ghPagesTempDir "firmware"
    New-Item -ItemType Directory -Path $firmwareDestDir -Force | Out-Null

    $models = @("N16R8", "N8R8", "N8R2", "N4R2")
    foreach ($m in $models) {
        $fFull = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_${m}_full.bin"
        $fApp = Join-Path $flasherDir "bin\RemoteMapper_ESP32S3_${m}_app.bin"
        if (Test-Path -LiteralPath $fFull) { Copy-Item $fFull "$firmwareDestDir\" -Force }
        if (Test-Path -LiteralPath $fApp) { Copy-Item $fApp "$firmwareDestDir\" -Force }
    }

    New-Item -ItemType File -Path (Join-Path $ghPagesTempDir ".nojekyll") -Force | Out-Null
    $remoteOrigin = (& git -C $rootDir remote get-url origin).Trim()

    Push-Location $ghPagesTempDir
    & git init -q
    & git config user.name "github-actions[bot]"
    & git config user.email "github-actions[bot]@users.noreply.github.com"
    & git checkout -b gh-pages -q
    & git add .
    & git commit -m "Deploy Web Serial Flasher with updated firmware binaries" -q
    & git remote add origin $remoteOrigin
    & git push origin gh-pages --force
    Pop-Location
    Write-Host "[SUCCESS] gh-pages deployed successfully!" -ForegroundColor Green
} finally {
    if (Test-Path -LiteralPath $ghPagesTempDir) {
        Remove-Item -Recurse -Force -LiteralPath $ghPagesTempDir -ErrorAction SilentlyContinue
    }
}
