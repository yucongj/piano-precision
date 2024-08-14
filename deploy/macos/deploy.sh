#!/bin/bash

set -e

# Execute this from the top-level directory of the project (the one
# that contains the .app bundle).  Supply the name of the application
# as argument.
#
# This now performs *only* the app deployment step - copying in
# libraries and setting up paths etc. It does not create a
# package. Use deploy-and-package.sh for that.

. deploy/metadata.sh

usage() {
    echo
    echo "Usage: $0 [<builddir>]"
    echo
    echo "  where <builddir> defaults to \"build\""
    echo 
    echo "For example,"
    echo
    echo "  \$ $0"
    echo "  to deploy an app built in the directory \"build\""
    echo
    echo "  \$ $0 build-release"
    echo "  to deploy an app built in the directory \"build-release\""
    echo
    echo "The app name <app> will be extracted from meson.build."
    echo
    echo "The executable must be already present in <builddir>/<app>, and its"
    echo "version number will be extracted from <builddir>/version.h."
    echo 
    echo "In all cases the target will be an app bundle called <app>.app in"
    echo "the current directory."
    echo
    exit 2
}

builddir="$1"
if [ -z "$builddir" ]; then
    builddir=build
elif [ -n "$2" ]; then
    usage
fi

set -u

if [ ! -f "$builddir/$full_name" ]; then
    echo "File $full_name not found in builddir $builddir"
    exit 2
fi

source="$full_app"

set -u

mkdir -p "$source/Contents/MacOS"
mkdir -p "$source/Contents/Resources"

cp -a "$builddir/$full_name" "$source/Contents/MacOS"

echo
echo "Copying in scores and recordings, if found."

deploy/macos/copy-scores.sh "$full_name"

echo
echo "Copying in icon."

cp "icons/piano-precision-macicon.icns" "$source/Contents/Resources"

echo
echo "Copying in frameworks and plugins from Qt installation directory."

deploy/macos/copy-qt.sh "$full_name" || exit 2

echo
echo "Fixing up paths."

deploy/macos/paths.sh "$full_name"

echo
echo "Copying in qt.conf to set local-only plugin paths."
echo "Make sure all necessary Qt plugins are in $source/Contents/plugins/*"
echo "You probably want platforms/, accessible/ and imageformats/ subdirectories."
cp deploy/macos/qt.conf "$source"/Contents/Resources/qt.conf

echo
echo "Copying in plugin load checker."
cp $builddir/vamp-plugin-load-checker "$source"/Contents/MacOS/

echo
echo "Copying in plugin server."
cp $builddir/piper-vamp-simple-server "$source"/Contents/MacOS/

echo
echo "Copying in piper convert tool."
cp $builddir/piper-convert "$source"/Contents/MacOS/

echo
echo "Copying in aligner plugin."
cp $builddir/score-aligner.dylib "$source"/Contents/Resources/

echo
echo "Copying in dummy aligner plugin."
cp $builddir/dummy-aligner.dylib "$source"/Contents/Resources/

echo
echo "Copying in lproj directories containing InfoPlist.strings translation files."
cp -r i18n/*.lproj "$source"/Contents/Resources/

echo
echo "Writing version $version in to bundle."
echo "(This should be a three-part number: major.minor.point)"

cat deploy/macos/Info.plist | \
    perl -p -e "s/[@]VERSION[@]/$version/g; s/[@]NAME[@]/$full_name/g; s/[@]IDENT[@]/$full_ident/g" deploy/macos/Info.plist \
    > "$source"/Contents/Info.plist

echo "Done: check $source/Contents/Info.plist for sanity please"
