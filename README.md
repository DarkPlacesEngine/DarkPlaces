# DarkPlaces Engine

DarkPlaces is a game engine based on the Quake 1 engine by id Software. It
improves and builds upon the original 1996 engine by adding modern rendering
features, and expanding upon the engine's native game code language QuakeC, as
well as supporting additional map and model formats.

Developed by LadyHavoc. See [CREDITS](CREDITS.md) for a list of contributors.

## Help/support

### IRC
#darkplaces on irc.anynet.org

### [Matrix](https://matrix.org/docs/guides/introduction)
Space: [#darkplaces:matrix.org](https://matrix.to/#/#darkplaces:matrix.org)

## Build instructions (WIP)

You will need the following packages regardless of platform:
* SDL2
* libjpeg
* libpng
* libvorbis
* libogg

### Windows (MSYS2):

1. Install MSYS2, found [here](https://www.msys2.org/).
2. Once you've installed MSYS2 and have fully updated it, open a MinGW64 terminal (***not an MSYS2 terminal***) and input the following command:

```
pacman -S --needed gcc make mingw-w64-x86_64-{toolchain,libjpeg-turbo,libpng,libogg,libvorbis,SDL2}
```

3. See [Unix instructions](#unix-(general)).

### macOS
1. Open a terminal and input `xcode-select --install`
2. Install [Homebrew](https://brew.sh)
3. In the same (or a different terminal), input the following command:

```
brew install sdl2 libjpeg-turbo libpng libvorbis curl
```

4. See [Unix instructions](#unix-(general)).

### Unix (General)

From a terminal, in the engine's root directory, input `make`. On macOS, input `make` with a target such as `make sdl-release`.

Input `make help` for options.

### Windows (Visual Studio)

Instructions coming soon.

## Contributing

[DarkPlaces Contributing Guidelines](CONTRIBUTING.md)

## Documentation

Doxygen: https://xonotic.org/doxygen/darkplaces
