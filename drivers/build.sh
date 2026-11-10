#!/bin/bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NDK="${ANDROID_NDK:-$HOME/Library/Android/sdk/ndk/26.1.10909125}"
TC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin"
API=23
OUT="$HERE/../app/src/main/jniLibs"
SRC="$HERE/src"
BUILD="$HERE/build"
COMPAT="$HERE/compat/android_compat.h"
ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64 x86}"

triple() {
  case "$1" in
    arm64-v8a) echo aarch64-linux-android ;;
    armeabi-v7a) echo armv7a-linux-androideabi ;;
    x86_64) echo x86_64-linux-android ;;
    x86) echo i686-linux-android ;;
  esac
}

build_libcups() {
  local abi="$1" t="$2" dir="$BUILD/$abi/cups"
  [ -f "$dir/cups/libcups.a" ] && return
  mkdir -p "$dir"
  rsync -a --delete --exclude .git "$SRC/cups/" "$dir/"
  (cd "$dir" && ./configure --host="$t" \
      CC="$TC/$t$API-clang" CXX="$TC/$t$API-clang++" AR="$TC/llvm-ar" RANLIB="$TC/llvm-ranlib" \
      --disable-shared --enable-static --disable-dbus --disable-pam --disable-gssapi \
      --with-dnssd=no --with-tls=no --disable-libusb --without-systemd --with-components=libcups \
      > "$BUILD/$abi/cups-configure.log" 2>&1)
  (cd "$dir/cups" && make -j8 OPTIM="-Os -include $COMPAT" libcups.a > "$BUILD/$abi/cups-make.log" 2>&1)
}

build_brlaser() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups" gen="$BUILD/$abi/brlaser"
  mkdir -p "$gen" "$OUT/$abi"
  printf '#define PACKAGE "brlaser"\n#define VERSION "6"\n' > "$gen/config.h"
  "$TC/$t$API-clang++" -std=c++11 -Os -fPIE -pie -static-libstdc++ \
    -include "$COMPAT" -I"$gen" -I"$cups" -I"$cups/cups" \
    "$SRC/brlaser/src/main.cc" "$SRC/brlaser/src/job.cc" "$SRC/brlaser/src/line.cc" "$SRC/brlaser/src/debug.cc" \
    "$cups/cups/libcups.a" -lz -Wl,--gc-sections -s \
    -o "$OUT/$abi/librastertobrlaser.so"
}

build_cehraster() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups"
  mkdir -p "$OUT/$abi"
  "$TC/$t$API-clang" -std=c99 -Os -fPIE -pie -include "$COMPAT" -I"$cups" \
    "$HERE/cehraster/cehraster.c" "$cups/cups/libcups.a" -lz -lm -Wl,--gc-sections -s \
    -o "$OUT/$abi/libcehraster.so"
}

apply_patches() {
  local p
  for p in "$HERE"/patches/splix-*.patch; do
    (cd "$SRC/splix-git" && patch -p1 -N --dry-run -s < "$p" >/dev/null 2>&1 && patch -p1 -N -s < "$p") || true
  done
  for p in "$HERE"/patches/escpr-*.patch; do
    (cd "$SRC/escpr" && patch -p1 -N --dry-run -s < "$p" >/dev/null 2>&1 && patch -p1 -N -s < "$p") || true
  done
  for p in "$HERE"/patches/hplip-*.patch; do
    (cd "$SRC/hplip" && patch -p1 -N --dry-run -s < "$p" >/dev/null 2>&1 && patch -p1 -N -s < "$p") || true
  done
}

build_splix() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups" jb="$SRC/jbigkit/libjbig" S="$SRC/splix-git"
  mkdir -p "$OUT/$abi"
  local objs="$BUILD/$abi/splix-obj"; mkdir -p "$objs"
  local files="rastertoqpdl request printer qpdl document core compress algorithm ppdfile page colors band bandplane cache rendering semaphore algo0x0d algo0x0e algo0x11 algo0x13 algo0x15"
  for f in $files; do
    "$TC/$t$API-clang++" -std=gnu++11 -Os -fPIE -w -DTHREADS=2 -DCACHESIZE=30 -include "$COMPAT" \
      -I"$S/include" -I"$jb" -I"$cups" -c "$S/src/$f.cpp" -o "$objs/$f.o"
  done
  for f in jbig85 jbig_ar; do
    "$TC/$t$API-clang" -Os -fPIE -w -c "$jb/$f.c" -o "$objs/$f.o"
  done
  "$TC/$t$API-clang++" -pie -static-libstdc++ "$objs"/*.o "$cups/cups/libcups.a" -lz -lm -Wl,--gc-sections -s \
    -o "$OUT/$abi/librastertoqpdl.so"
}

build_gutenprint() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups" G="$BUILD/$abi/gutenprint" S="$SRC/gutenprint"
  mkdir -p "$OUT/$abi" "$G"
  if [ ! -f "$G/src/main/.libs/libgutenprint.a" ]; then
    (cd "$G" && "$S/configure" --host="$t" \
        CC="$TC/$t$API-clang" AR="$TC/llvm-ar" RANLIB="$TC/llvm-ranlib" \
        --disable-shared --enable-static --disable-libgutenprintui2 --without-gimp2 \
        --disable-testpattern --disable-test --without-doc --disable-escputil --without-ghostscript \
        --without-foomatic --without-foomatic3 --disable-samples --disable-user-guide \
        --disable-cups-ppds --without-cups > "$BUILD/$abi/gutenprint-configure.log" 2>&1)
    make -C "$G/src/main" -j8 > "$BUILD/$abi/gutenprint-make.log" 2>&1
  fi
  local flags="-Os -fPIE -pie -include $COMPAT -DHAVE_CONFIG_H -DHAVE_LIBZ=1 -DALL_LINGUAS=\"\" -DBASE_VERSION=\"5.3\" -DSBINDIR=\"/x/\" \
    -I$G -I$G/include -I$S/include -I$S/src/cups -I$cups"
  local libs="$G/src/main/.libs/libgutenprint.a $cups/cups/libcups.a -lz -lm -Wl,--gc-sections -s"
  "$TC/$t$API-clang" $flags "$S/src/cups/rastertogutenprint.c" "$HERE/compat/gp_i18n.c" $libs \
    -o "$OUT/$abi/librastertogutenprint.so"
  "$TC/$t$API-clang" $flags "$S/src/cups/gutenprint.c" "$S/src/cups/genppd.c" "$HERE/compat/gp_i18n.c" $libs \
    -o "$OUT/$abi/libgutenprintppd.so"
}

build_jpeg() {
  local abi="$1" dir="$BUILD/$abi/jpeg" cm="$HOME/Library/Android/sdk/cmake/3.22.1/bin"
  [ -f "$dir/libjpeg.a" ] && return
  mkdir -p "$dir"
  (cd "$dir" && "$cm/cmake" -G Ninja -DCMAKE_MAKE_PROGRAM="$cm/ninja" \
      -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI="$abi" \
      -DANDROID_PLATFORM=android-$API -DENABLE_SHARED=0 -DENABLE_STATIC=1 -DWITH_TURBOJPEG=0 \
      -DWITH_SIMD=0 -DCMAKE_BUILD_TYPE=Release "$SRC/libjpeg-turbo" > cfg.log 2>&1 \
    && "$cm/cmake" --build . -j8 > build.log 2>&1)
}

build_hpcups() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups" J="$BUILD/$abi/jpeg" H="$SRC/hplip" objs="$BUILD/$abi/hpcups-obj"
  mkdir -p "$OUT/$abi" "$objs"
  build_jpeg "$abi"
  local inc="-I$H/prnt/hpcups -I$H/common -I$H -I$H/prnt/hpijs -I$H/ip -I$H/io/hpmud -I$J -I$SRC/libjpeg-turbo -I$cups"
  local def="-DDISABLE_IMAGEPROCESSOR -DAPDK_LITTLE_ENDIAN -DCONFDIR=\"/etc/hp\" -DSTATEDIR=\"/var/lib/hp\""
  local f n
  for f in $(grep -oE "prnt/hpcups/[A-Za-z0-9_]+\.cpp" "$H/Makefile.am" | sort -u); do
    n="$(basename "$f" .cpp)"
    "$TC/$t$API-clang++" -std=gnu++11 -Os -fPIE -w $def -include "$COMPAT" $inc -c "$H/$f" -o "$objs/$n.o"
  done
  for f in prnt/hpcups/jccolor.c prnt/hpcups/jdatadbf.c common/utils.c; do
    n="c_$(basename "$f" .c)"
    "$TC/$t$API-clang" -Os -fPIE -w $def -include "$COMPAT" $inc -c "$H/$f" -o "$objs/$n.o"
  done
  "$TC/$t$API-clang++" -pie -static-libstdc++ "$objs"/*.o "$J/libjpeg.a" "$cups/cups/libcups.a" -lz -lm -ldl \
    -Wl,--gc-sections -s -o "$OUT/$abi/libhpcups.so"
}

build_escpr() {
  local abi="$1" t="$2" cups="$BUILD/$abi/cups" E="$SRC/escpr" objs="$BUILD/$abi/escpr-obj"
  mkdir -p "$OUT/$abi" "$objs"
  printf '#define PACKAGE "epson-inkjet-printer-escpr"\n#define VERSION "1.7.17"\n#define PACKAGE_STRING "epson-inkjet-printer-escpr 1.7.17"\n' > "$objs/config.h"
  local d="-DGCOMSW_PRT_USE_USB -DGCOMSW_PRT_USE_LPR -DGCOMSW_PRT_USE_RAW -DGCOMSW_CMD_ESCPAGE -DGCOMSW_EL_CDLABEL -DGCOMSW_EF_MAINTE -DEPS_FILTER -DGCOMSW_ASYNCJOB_SUPPORT -DGCOMSW_RAWFILE_SUPPORT -DHAVE_CONFIG_H"
  local cf="-Os -fPIE -pie -w -DHAVE_CONFIG_H -Wno-error=implicit-function-declaration -Wno-error=implicit-int -fcommon -include $COMPAT -I$objs -I$E/lib -I$E/src -I$cups"
  local core="$(ls "$E"/lib/*.c) $E/src/filter.c $E/src/debug.c $E/src/str.c $E/src/err.c $E/src/mem.c $E/src/linux_cmn.c $E/src/xfifo.c"
  "$TC/$t$API-clang" $cf $d $core "$cups/cups/libcups.a" -lz -lm -Wl,--gc-sections -s -o "$OUT/$abi/libescprcore.so"
  "$TC/$t$API-clang" $cf -DCUPS_FILTER_NAME=\"libescprcore.so\" -DCUPS_FILTER_PATH=\"/x\" "$E/src/wrapper.c" \
    "$cups/cups/libcups.a" -lz -lm -Wl,--gc-sections -s -o "$OUT/$abi/libepson-escpr-wrapper.so"
}

apply_patches
build_captjob() {
  local abi="$1" t="$2" d="$HERE/capt"
  mkdir -p "$OUT/$abi"
  "$TC/$t$API-clang" -std=gnu99 -Os -fPIE -pie -w -I"$d" -I"$d/captdriver" \
    "$d/capt_main.c" "$d"/captdriver/{capt-command,capt-status,generic-ops,hiscoa-common,hiscoa-compress,paper,printer,prn_lbp2900}.c \
    -llog -Wl,--gc-sections -s -o "$OUT/$abi/libcaptjob.so"
}

for abi in $ABIS; do
  t="$(triple "$abi")"
  echo "== $abi"
  build_libcups "$abi" "$t"
  build_brlaser "$abi" "$t"
  build_cehraster "$abi" "$t"
  build_splix "$abi" "$t"
  build_gutenprint "$abi" "$t"
  build_hpcups "$abi" "$t"
  build_escpr "$abi" "$t"
  build_captjob "$abi" "$t"
done
ls -la "$OUT"/*/lib*.so
