# Flappy Bird

An attempt to recreate Flappy Bird game using C99 and Raylib. Assets is from [samuelcust/flappy-bird-assets](https://github.com/samuelcust/flappy-bird-assets).
Web version is available [here](https://misterabdul.moe/flappy-bird).

## Requirement

- Platform: Desktop (tested on Fedora 43) & Web
- Compiler: C99 compiler ([EMSDK's emcc](https://github.com/emscripten-core/emsdk) if you want to build web version).
- Dependency: C99 standard library, [raylib](https://github.com/raysan5/raylib).

## Building

- [EMSDK](https://github.com/emscripten-core/emsdk) is installed at `/usr/local/src/emsdk`, as of writing this the SDK is at version 4.0.21.
- [Raylib](https://github.com/raysan5/raylib) is installed at `/usr/local/src/raylib`, as of writing this the library is at version 5.5.

### Building raylib

The script I use to build the raylib:

```sh
$ cd /usr/local/src/raylib
$ gmake # desktop variant: libraylib.a
$ EMSDK_PATH="/usr/local/src/emsdk" gmake PLATFORM="PLATFORM_WEB" -B # web variant: libraylib.web.a
```

### Building the game

Example script to build the game, desktop version:

```sh
$ PLATFORM="desktop" CC="clang" CFLAGS="-g" LDFLAGS="-g" ./configure
$ gmake
```

Web version:

```sh
$ PLATFORM="web" CC="emcc" CFLAGS="-O2" LDFLAGS="-O2" RAYLIB_PATH="/usr/local/src/raylib" ./configure
$ gmake
```
