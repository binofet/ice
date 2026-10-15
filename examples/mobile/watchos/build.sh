#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
platform=${1:-simulator}
case "$platform" in
 simulator) sdk=watchsimulator; signing=NO;;
 device) sdk=watchos; signing=YES;;
 *) echo 'Usage: build.sh [simulator|device]' >&2; exit 2;;
esac
cmake -S "$project_root/examples/mobile" -B "$project_root/build/mobile/watchos-$platform" -G Xcode \
 -DCMAKE_SYSTEM_NAME=watchOS -DCMAKE_OSX_SYSROOT="$sdk" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.0 \
 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED="$signing" \
 -DICE_MOBILE_BUNDLE_ID="${ICE_BUNDLE_ID:-com.binofet.ice.companion.watch}" \
 -DICE_DEVELOPMENT_TEAM="${ICE_DEVELOPMENT_TEAM:-}"
if [ "$platform" = device ]; then
 xcodebuild -project "$project_root/build/mobile/watchos-device/ICEMobileProbe.xcodeproj" -scheme ICEWatchProbe -configuration Debug -allowProvisioningUpdates -destination "${ICE_WATCH_DESTINATION:-generic/platform=watchOS}"
else
 cmake --build "$project_root/build/mobile/watchos-simulator" --config Debug
fi
