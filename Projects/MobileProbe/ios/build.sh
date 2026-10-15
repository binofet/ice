#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
platform=${1:-simulator}
case "$platform" in
 simulator) sdk=iphonesimulator;;
 device) sdk=iphoneos;;
 *) printf '%s\n' 'Usage: build.sh [simulator|device]' >&2;exit 2;;
esac
cmake -S "$project_root/Projects/MobileProbe" -B "$project_root/build/ios-$platform" -G Xcode \
 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$sdk" -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
 -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$project_root/build/ios-$platform" --config Debug
