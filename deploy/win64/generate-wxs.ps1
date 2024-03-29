
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

. deploy\metadata.ps1

$redist_parent_dir = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\"

$redists = @(Get-ChildItem -Path $redist_parent_dir -Name -Include 14.* -Attributes Directory)

if (!$redists) {
    echo "ERROR: No 14.x redistributable directories found under $redist_parent_dir"
    exit 1
}

"Redists are: $redists"

$redist_ver = $redists[-1]

$wxs = "deploy\win64\$full_kebab.wxs"
$in = "deploy\win64\wxs.in"

$redist_dir="$redist_parent_dir\$redist_ver\x64\Microsoft.VC142.CRT"

echo "Generating $wxs..."
echo " ...for app version $version"
echo " ...for redist version $redist_ver"
echo " ...from $in"
echo ""

if (!(Test-Path -Path $redist_dir -PathType Container)) {
    echo "ERROR: Redistributable directory $redist_dir not found"
    exit 1
}

if (!(Test-Path -Path $in -PathType Leaf)) {
    echo "ERROR: Input file $in not found"
    exit 1
}

$suppress_re = "^(C major |TheBlues)"

$component_refs = @()
$guid_base = 'e260ca94-833d-47a0-{0}-a658022bd0a7'
$global:guid_count = 1000
function GetNextId () {
    $guid_base -f $global:guid_count
    $global:guid_count = $global:guid_count + 1
}

echo ""
echo "Scores:"

$scoredir = "..\piano-precision-data\Scores"
$scores = Get-ChildItem -path $scoredir -attributes Directory

$scorespec = $scores.foreach{
    $score = $_
    if (!($score.Name -match $suppress_re)) {
        $score.Name | Out-Host
        $id = $score.Name -Replace "[ .,()-]", ""
        $files = Get-ChildItem -path "$scoredir\$score\*" -File -Include "$score*"
        $xfrags = $files.foreach{
            $file = $_
            $fxml = [xml] '<File Id="" Name="" Source=""/>'
            $fxml.File.Id = $file.Name -Replace "[ .,()-]",""
            $fxml.File.Name = $file.Name
            $fxml.File.Source = $file.FullName
            $fxml.OuterXml
        }
        $xtext = $xfrags -join "`r`n"
        $dxml = [xml] "<Directory Id='' Name=''><Component Id='' Guid=''>$xtext</Component></Directory>"
        $did = "SCOD_$id"
        $cid = "SCOC_$id"
        $dxml.Directory.Id = $did
        $dxml.Directory.Name = $score.Name
        $dxml.Directory.Component.Guid = $(GetNextId)
        $dxml.Directory.Component.Id = $cid
        $component_refs += "<ComponentRef Id='$cid'/>"
        $dxml.OuterXml
    }
} -join "`r`n"

echo ""
echo "Recordings:"

$recordingdir = "..\piano-precision-data\Recordings"
$recordings = Get-ChildItem -path $recordingdir -attributes Directory

$recordingspec = $recordings.foreach{
    $recording = $_
    if (!($recording.Name -match $suppress_re)) {
        $recording.Name | Out-Host
        $dxml = [xml] @"
  <Directory Id="" Name="">
    <Component Id="" Guid="">
      <File Id="" Name="" Source=""/>
    </Component>
  </Directory>
"@
        $id = $recording.Name -Replace "[ .,()-]", ""
        $did = "RECD_$id"
        $cid = "RECC_$id"
        $fid = "RECF_$id"
        $fname = $(Get-ChildItem -Path "$recordingdir\$recording\*" -File -Include "*.opus")[0].Name
        $dxml.Directory.Id = $did
        $dxml.Directory.Name = $recording.Name
        $dxml.Directory.Component.Id = $cid
        $dxml.Directory.Component.Guid = $(GetNextId)
        $dxml.Directory.Component.File.Id = $fid
        $dxml.Directory.Component.File.Name = $fname
        $fsrc = $recording.FullName + "\" + $fname
        if (!(Test-Path -Path $fsrc -PathType Leaf)) {
            "ERROR: Recording $fsrc not found" | Out-Host
            exit 1
        }
        $dxml.Directory.Component.File.Source = $fsrc
        $component_refs += "<ComponentRef Id='$cid'/>"
        $dxml.OuterXml
    }
} -join "`r`n"

echo ""

$component_ref_text = $component_refs -join ""

(Get-Content $in) -replace '@VERSION@', $version `
  -replace '@REDIST_VER@', $redist_ver `
  -replace '@NAME@', $full_name `
  -replace '@CONDENSED@', $full_condensed `
  -replace '@SCORES@', $scorespec `
  -replace '@RECORDINGS@', $recordingspec `
  -replace '@COMPONENT_REFS@', $component_ref_text `
  -replace '@W@', '<!-- DO NOT EDIT THIS FILE: it is auto-generated -->' `
  | Out-File -encoding ASCII $wxs

echo "Done"
