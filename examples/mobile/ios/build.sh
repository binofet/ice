#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
platform=${1:-simulator}
case "$platform" in
 simulator) sdk=iphonesimulator;;
 device) sdk=iphoneos;;
 *) printf '%s\n' 'Usage: build.sh [simulator|device]' >&2;exit 2;;
esac
team=${ICE_DEVELOPMENT_TEAM:-}
signing=NO
if [ "$platform" = device ] && [ -n "$team" ]; then signing=YES; fi
cmake -S "$project_root/examples/mobile" -B "$project_root/build/mobile/ios-$platform" -G Xcode \
 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$sdk" -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
 -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED="$signing" \
 -DICE_MOBILE_BUNDLE_ID="${ICE_BUNDLE_ID:-org.ice.MobileProbe}" -DICE_DEVELOPMENT_TEAM="$team"
if [ "$signing" = YES ]; then
 if [ -n "${ICE_DEVICE_UDID:-}" ]; then
  xcodebuild -project "$project_root/build/mobile/ios-device/ICEMobileProbe.xcodeproj" -scheme ICEMobileProbe -configuration Debug -allowProvisioningUpdates -allowProvisioningDeviceRegistration -destination "id=$ICE_DEVICE_UDID"
 else
  xcodebuild -project "$project_root/build/mobile/ios-device/ICEMobileProbe.xcodeproj" -scheme ICEMobileProbe -configuration Debug -allowProvisioningUpdates -destination "generic/platform=iOS"
 fi
else
 cmake --build "$project_root/build/mobile/ios-$platform" --config Debug
fi
