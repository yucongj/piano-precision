#!/bin/bash

set -eu

app="$1"
if [ -z "$app" ]; then
    echo "Usage: $0 <appname>"
    echo "Provide appname without the .app extension, please"
    exit 2
fi

sdir="$app.app/Contents/Resources/Scores"
rdir="$app.app/Contents/Resources/Recordings"

mkdir -p "$sdir"
mkdir -p "$rdir"

datadir=../piano-precision-data
ssrc="$datadir/Scores"
rsrc="$datadir/Recordings"

if [ -d "$datadir" ]; then
    echo
    echo "Found data directory at $datadir"
    echo
    echo "Copying scores from $ssrc to $sdir..."
    cp -r "$ssrc"/* "$sdir"/
    echo
    echo "Copying recordings from $rsrc to $rdir..."
    cp -r "$rsrc"/* "$rdir"/
    echo
    echo "Removing .wav files where .opus found..."
    for wav in "$rdir"/*/*.wav ; do
	opus=${wav%wav}opus
	if [ ! -f "$opus" ]; then
	    echo "WARNING: No corresponding .opus for .wav file $wav, ignoring"
	else
	    rm "$wav"
	fi
    done
    echo
    echo "Removing trivial scores (C major scales)..."
    rm -r "$sdir/C major"*
    rm -r "$rdir/C major"*
    echo
    echo "Done"
else
    echo
    echo "Failed to find data directory at $datadir"
    echo "Not copying scores or recordings"
fi

