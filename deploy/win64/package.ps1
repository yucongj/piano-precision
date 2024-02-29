# Run this from the project root

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$sign = 0
if ($args[0] -eq "sign") {
    $sign = 1
} elseif ($args.count -gt 0 -and $args[0] -ne "") {
    "Usage: package [sign]"
    exit 2
}

. deploy\metadata.ps1

$kitdir = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"
$wixdir = "C:\Program Files (x86)\WiX Toolset v3.14\bin"

if (! (Test-Path -Path $kitdir -PathType Container)) {
   "ERROR: Windows Kit directory $kitdir not found"
   exit 2
}

if (! (Test-Path -Path $wixdir -PathType Container)) {
   "ERROR: WiX Toolset directory $wixdir not found"
   exit 2
}

if (! (& "deploy\win64\generate-wxs.ps1")) {
  "ERROR: WiX file generation failed"
  exit 2
}

$name = "Christopher Cannam"

if ($sign) {
   "Signing executables"
   &"$kitdir\signtool" sign /v /debug /n "$name" /t http://time.certum.pl /fd sha256 /a build_win64\*.exe
}

"Packaging"

if (Test-Path -Path "$full_kebab.msi") {
    rm $full_kebab.msi
}

&"$wixdir\candle" -v deploy\win64\"$full_kebab.wxs"
&"$wixdir\light" -b . -ext WixUIExtension -ext WixUtilExtension -v "$full_kebab.wixobj"
rm "$full_kebab.wixobj"
rm "$full_kebab.wixpdb"

if ($sign) {
    "Signing packages"
    &"$kitdir\signtool" sign /v /n "$name" /t http://time.certum.pl /fd sha256 /a *.msi
    &"$kitdir\signtool" verify /pa "$full_kebab.msi"
}

mkdir -Force packages
mv -Force "$full_kebab.msi" "packages\$full_versioned 64bit.msi"

"Done"


