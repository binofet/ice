#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
sdk=${ANDROID_SDK_ROOT:-"$HOME/Library/Android/sdk"}
ndk=${ANDROID_NDK_ROOT:-"$project_root/build/android-sdk/ndk/26.3.11579264"}
standard=${ICE_CXX_STANDARD:-23}
build="$project_root/build/xr/android-$standard"
cache="$project_root/build/xr-deps"
version=1.1.63
archive="$cache/openxr_loader_for_android-$version.aar"
# Each language-mode build owns its extraction, so C++20/23 can build concurrently.
loader="$build/openxr-$version"
archive_sha=622419d2f6741c3443a3beb4779af0764318edd01830de967f24c741ebcded73
tools="$sdk/build-tools/34.0.0"
key="$project_root/build/xr/debug.keystore"
export JAVA_HOME=${JAVA_HOME:-"/Applications/Android Studio.app/Contents/jbr/Contents/Home"}
mkdir -p "$cache" "$loader"
if [ ! -f "$archive" ]; then
    curl --fail --location --retry 2 "https://repo.maven.apache.org/maven2/org/khronos/openxr/openxr_loader_for_android/$version/openxr_loader_for_android-$version.aar" -o "$archive"
fi
printf '%s  %s\n' "$archive_sha" "$archive" | shasum -a 256 -c -
unzip -oq "$archive" -d "$loader"
cmake -S "$project_root/examples/xr" -B "$build" -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Debug -DICE_CXX_STANDARD="$standard" -DICE_OPENXR_AAR_DIR="$loader"
cmake --build "$build" --parallel 4
mkdir -p "$build/classes" "$build/dex"
"$JAVA_HOME/bin/javac" -source 8 -target 8 -bootclasspath "$sdk/platforms/android-34/android.jar" -d "$build/classes" "$project_root/examples/xr/android/CaptureActivity.java"
"$tools/d8" --min-api 29 --lib "$sdk/platforms/android-34/android.jar" --output "$build/dex" "$build/classes/org/ice/xrprobe/"*.class
mkdir -p "$build/apk/lib/arm64-v8a" "$build/assets/licenses"
cp "$build/libice_xr_probe.so" "$build/apk/lib/arm64-v8a/"
cp "$loader/jni/arm64-v8a/libopenxr_loader.so" "$build/apk/lib/arm64-v8a/"
cp "$project_root/examples/xr/licenses/OpenXR-1.1.63-APACHE-2.0.txt" "$build/assets/licenses/"
cp "$project_root/engine/3rdparty/glm/copying.txt" "$build/assets/licenses/GLM.txt"
cp "$project_root/LICENSE" "$build/assets/licenses/ICE-MIT.txt"
cp "$ndk/sources/android/native_app_glue/NOTICE" "$build/assets/licenses/Android-Native-App-Glue-NOTICE.txt"
cp "$ndk/NOTICE.toolchain" "$build/assets/licenses/NDK-Toolchain-NOTICE.txt"
"$tools/aapt2" link --manifest "$project_root/examples/xr/android/AndroidManifest.xml" -A "$build/assets" -I "$sdk/platforms/android-34/android.jar" -o "$build/unsigned.apk"
(cd "$build/apk" && zip -q "$build/unsigned.apk" lib/arm64-v8a/libice_xr_probe.so lib/arm64-v8a/libopenxr_loader.so)
(cd "$build/dex" && zip -q "$build/unsigned.apk" classes.dex)
"$tools/zipalign" -f 4 "$build/unsigned.apk" "$build/aligned.apk"
if [ ! -f "$key" ]; then
    "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$key" -storepass android -keypass android -alias androiddebugkey -dname 'CN=ICE XR Debug' -keyalg RSA -validity 3650
fi
"$tools/apksigner" sign --ks "$key" --ks-pass pass:android --out "$build/ICE-XR-Lab.apk" "$build/aligned.apk"
"$tools/apksigner" verify "$build/ICE-XR-Lab.apk"
printf '%s\n' "$build/ICE-XR-Lab.apk"
