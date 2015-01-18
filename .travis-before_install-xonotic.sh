#!/bin/sh

set -ex

if [ "`uname`" = 'Linux' ]; then
  sudo apt-get update -qq
fi

for os in "$@"; do
  git archive --format=tar --remote=git://de.git.xonotic.org/xonotic/xonotic.git \
    --prefix=".deps/${os}/" master:"misc/builddeps/${os}" | tar xvf -

  case "$os" in
    linux32)
      # Prepare an i386 chroot. This is required as we otherwise can't install
      # our dependencies to be able to compile a 32bit binary. Ubuntu...
      chroot="$PWD"/buildroot.i386
      mkdir -p "$chroot$PWD"
      sudo apt-get install -y debootstrap
      sudo i386 debootstrap --arch=i386 precise "$chroot"
      sudo mount --rbind "$PWD" "$chroot$PWD"
      sudo i386 chroot "$chroot" apt-get install -y \
        build-essential
      # Now install our dependencies.
      sudo i386 chroot "$chroot" apt-get install -y \
        libxpm-dev libsdl1.2-dev libxxf86vm-dev
      wget https://www.libsdl.org/release/SDL2-2.0.3.tar.gz
      tar xf SDL2-2.0.3.tar.gz
      (
      cd SDL2-2.0.3
      sudo i386 chroot "$chroot" sh -c "cd $PWD && ./configure --enable-static --disable-shared"
      sudo i386 chroot "$chroot" make -C "$PWD"
      sudo i386 chroot "$chroot" make -C "$PWD" install
      )
      ;;
    linux64)
      sudo apt-get install -y \
        libxpm-dev libsdl1.2-dev libxxf86vm-dev
      wget https://www.libsdl.org/release/SDL2-2.0.3.tar.gz
      tar xf SDL2-2.0.3.tar.gz
      (
      cd SDL2-2.0.3
      ./configure --enable-static --disable-shared
      make
      sudo make install
      )
      ;;
    win32)
      git archive --format=tar --remote=git://de.git.xonotic.org/xonotic/xonotic.git \
        --prefix=".icons/" master:"misc/logos/icons_ico" | tar xvf -
      mv .icons/xonotic.ico darkplaces.ico
      wget -qO- http://beta.xonotic.org/win-builds.org/cross_toolchain_32.tar.xz | sudo tar xaJvf - -C/ opt/cross_toolchain_32
      ;;
    win64)
      git archive --format=tar --remote=git://de.git.xonotic.org/xonotic/xonotic.git \
        --prefix=".icons/" master:"misc/logos/icons_ico" | tar xvf -
      mv .icons/xonotic.ico darkplaces.ico
      wget -qO- http://beta.xonotic.org/win-builds.org/cross_toolchain_64.tar.xz | sudo tar xvJf - -C/ opt/cross_toolchain_64
      ;;
    osx)
      git archive --format=tar --remote=git://de.git.xonotic.org/xonotic/xonotic.git \
        --prefix=SDL2.framework/ master:misc/buildfiles/osx/Xonotic.app/Contents/Frameworks/SDL2.framework | tar xvf -
      ;;
  esac
done

for X in .deps/*; do
  rsync --remove-source-files -aL "$X"/*/ "$X"/ || true
done
