
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$all_info = (meson introspect --projectinfo meson.build | ConvertFrom-Json)
$rdomain = "uk.co.particularprograms"

$version = $all_info.version

switch -Regex ($version) {
    '^1\.0$' {
        "## Error: Version $version must not be used - it is the App Store sandbox default"
        exit 2
    }
    '^\d+\.\d+$' {
        $version = "$version.0"
        break
    }
    '^\d+\.\d+\.\d+$' {
        break
    }
    default {    
        "## Error: Version $version is neither two- nor three-part number"
        exit 2
    }
}

$full_name = $all_info.descriptive_name

$full_app = "$full_name.app"
$full_condensed = $full_name -replace " ",""
$full_kebab = $full_name.ToLower() -replace " ","-"
$full_ident = "$rdomain.$full_condensed"
$full_versioned = "$full_name $version"


