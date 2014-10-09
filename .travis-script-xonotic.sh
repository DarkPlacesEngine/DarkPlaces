#!/bin/sh

set -e

openssl aes-256-cbc -K $encrypted_eeb6f7a14a8e_key -iv $encrypted_eeb6f7a14a8e_iv -in .travis-id_rsa-xonotic -out id_rsa-xonotic -d

set -x

chmod 0600 id_rsa-xonotic
# ssh-keygen -y -f id_rsa-xonotic

rev=`git rev-parse HEAD`

sftp -oStrictHostKeyChecking=no -i id_rsa-xonotic -P 2222 -b - autobuild-bin-uploader@beta.xonotic.org <<EOF || true
mkdir ${rev}
EOF

for os in "$@"; do

  deps=".deps/${os}"
  case "${os}" in
    linux32)
      chroot="sudo chroot ${PWD}/buildroot.i386"
      makeflags='STRIP=: CC="${CC} -m32 -march=i686 -g1 -I../../../${deps}/include -L../../../${deps}/lib -DSUPPORTIPV6" SDL_CONFIG=sdl2-config LIB_JPEG=../../../${deps}/lib/libjpeg.a LIB_CRYPTO="../../../${deps}/lib/libd0_blind_id.a ../../../${deps}/lib/libgmp.a" DP_LINK_ZLIB=shared DP_LINK_JPEG=dlopen DP_LINK_ODE=dlopen DP_LINK_CRYPTO=shared DP_LINK_CRYPTO_RIJNDAEL=dlopen'
      maketargets='release'
      outputs='darkplaces-glx:xonotic-linux32-glx darkplaces-sdl:xonotic-linux32-sdl darkplaces-dedicated:xonotic-linux32-dedicated'
      ;;
    linux64)
      chroot=
      makeflags='STRIP=: CC="${CC} -m64 -g1 -I../../../${deps}/include -L../../../${deps}/lib -DSUPPORTIPV6" SDL_CONFIG=sdl2-config LIB_JPEG=../../../${deps}/lib/libjpeg.a LIB_CRYPTO="../../../${deps}/lib/libd0_blind_id.a ../../../${deps}/lib/libgmp.a" DP_LINK_ZLIB=shared DP_LINK_JPEG=dlopen DP_LINK_ODE=dlopen DP_LINK_CRYPTO=shared DP_LINK_CRYPTO_RIJNDAEL=dlopen'
      maketargets='release'
      outputs='darkplaces-glx:xonotic-linux64-glx darkplaces-sdl:xonotic-linux64-sdl darkplaces-dedicated:xonotic-linux64-dedicated'
      ;;
    win32)
      chroot=
      makeflags='STRIP=: DP_MAKE_TARGET=mingw UNAME=MINGW32 CC="i686-w64-mingw32-gcc -g1 -Wl,--dynamicbase -Wl,--nxcompat -I../../../${deps}/include -L../../../${deps}/lib -DUSE_WSPIAPI_H -DSUPPORTIPV6" WINDRES="i686-w64-mingw32-windres" SDL_CONFIG="../../../${deps}/bin/sdl2-config" DP_LINK_ZLIB=dlopen DP_LINK_JPEG=dlopen DP_LINK_ODE=dlopen DP_LINK_CRYPTO=dlopen DP_LINK_CRYPTO_RIJNDAEL=dlopen WIN32RELEASE=1 D3D=1'
      maketargets='release'
      outputs='darkplaces.exe:xonotic.exe darkplaces-sdl.exe:xonotic-sdl.exe darkplaces-dedicated.exe:xonotic-dedicated.exe'
      ;;
    win64)
      chroot=
      makeflags='STRIP=: DP_MAKE_TARGET=mingw UNAME=MINGW32 CC="x86_64-w64-mingw32-gcc -g1 -Wl,--dynamicbase -Wl,--nxcompat -I../../../${deps}/include -L../../../${deps}/lib -DSUPPORTIPV6" WINDRES="x86_64-w64-mingw32-windres" SDL_CONFIG="../../../${deps}/bin/sdl2-config" DP_LINK_ZLIB=dlopen DP_LINK_JPEG=dlopen DP_LINK_ODE=dlopen DP_LINK_CRYPTO=dlopen DP_LINK_CRYPTO_RIJNDAEL=dlopen WIN64RELEASE=1 D3D=1'
      maketargets='release'
      outputs='darkplaces.exe:xonotic-x64.exe darkplaces-sdl.exe:xonotic-x64-sdl.exe darkplaces-dedicated.exe:xonotic-x64-dedicated.exe'
      ;;
    osx)
      chroot=
      makeflags='STRIP=: CC="gcc -g1 -arch x86_64 -arch i386 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk -mmacosx-version-min=10.5 -I../../../${deps}/include -L../../../${deps}/lib -DSUPPORTIPV6" SDLCONFIG_MACOSXCFLAGS="-I${PWD}/SDL2.framework/Headers" SDLCONFIG_MACOSXLIBS="-F${PWD} -framework SDL2 -framework Cocoa -I${PWD}/SDL2.framework/Headers" SDLCONFIG_MACOSXSTATICLIBS="-F${PWD} -framework SDL2 -framework Cocoa -I${PWD}/SDL2.framework/Headers" DP_LINK_ZLIB=shared DP_LINK_JPEG=dlopen DP_LINK_ODE=dlopen DP_LINK_CRYPTO=dlopen DP_LINK_CRYPTO_RIJNDAEL=dlopen'
      maketargets='sv-release sdl-release'
      outputs='darkplaces-sdl:xonotic-osx-sdl-bin darkplaces-dedicated:xonotic-osx-dedicated'
      ;;
  esac

  (
  trap "${chroot} make -C ${PWD} ${makeflags} clean" EXIT
  eval "${chroot} make -C ${PWD} ${makeflags} ${maketargets}"
  for o in $outputs; do
    src=${o%%:*}
    dst=${o#*:}
    sftp -oStrictHostKeyChecking=no -i id_rsa-xonotic -P 2222 -b - autobuild-bin-uploader@beta.xonotic.org <<EOF
put ${src} ${rev}/${dst}
EOF
  done
  )

done
