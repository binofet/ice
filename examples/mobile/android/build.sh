#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
sdk=${ANDROID_SDK_ROOT:-"$HOME/Library/Android/sdk"}
ndk=${ANDROID_NDK_ROOT:-"$project_root/build/android-sdk/ndk/26.3.11579264"}
build="$project_root/build/mobile/android-probe"
tools="$sdk/build-tools/34.0.0"
export JAVA_HOME=${JAVA_HOME:-"/Applications/Android Studio.app/Contents/jbr/Contents/Home"}
cmake -S "$project_root/examples/mobile" -B "$build" -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build" -j6
mkdir -p "$build/apk/lib/arm64-v8a"
cp "$build/libice_mobile_probe.so" "$build/apk/lib/arm64-v8a/"
"$tools/aapt" package -f -S "$project_root/examples/mobile/android/res" -M "$project_root/examples/mobile/android/AndroidManifest.xml" -I "$sdk/platforms/android-34/android.jar" -F "$build/unsigned.apk"
(cd "$build/apk" && zip -q "$build/unsigned.apk" lib/arm64-v8a/libice_mobile_probe.so)
"$tools/zipalign" -f 4 "$build/unsigned.apk" "$build/aligned.apk"
# Generate an isolated debug key; no developer credentials or release signing involved.
if [ ! -f "$build/debug.keystore" ]; then
 "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$build/debug.keystore" -storepass android -keypass android -alias androiddebugkey -dname 'CN=ICE Debug' -keyalg RSA -validity 3650
fi
"$tools/apksigner" sign --ks "$build/debug.keystore" --ks-pass pass:android --out "$build/ICE-Mobile-Probe.apk" "$build/aligned.apk"
"$tools/apksigner" verify "$build/ICE-Mobile-Probe.apk"
printf '%s\n' "$build/ICE-Mobile-Probe.apk"
