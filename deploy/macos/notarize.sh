#!/bin/bash

## The following assumes we have generated an app password at
## appleid.apple.com and then stored it to keychain id "altool" using
## e.g.
## security add-generic-password -a "cannam+apple@all-day-breakfast.com" \
##   -w "generated-app-password" -s "altool"

## NB to verify:
# spctl -a -v "/Applications/Application.app"

user="appstore@particularprograms.co.uk"
team_id="73F996B92S"

set -eu

. deploy/metadata.sh

bundleid="$full_ident"
dmg="$full_dmg"

echo
echo "Uploading for notarization..."

xcrun notarytool submit \
    "$dmg" \
    --apple-id "$user" \
    --team-id "$team_id" \
    --keychain-profile notarytool \
    --wait --progress

echo
echo "Stapling to package..."

xcrun stapler staple "$dmg" || exit 1

