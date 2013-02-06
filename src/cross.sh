#!/bin/sh
arch="$1"
shift
case $arch in
    64)
        make CXX=x86_64-w64-mingw32-g++ STRIP=x86_64-w64-mingw32-strip WINDRES=x86_64-w64-mingw32-windres PLATFORM=MINGW64 $*
        ;;
    32)
        make CXX=i686-w64-mingw32-g++ STRIP=i686-w64-mingw32-strip WINDRES=i686-w64-mingw32-windres PLATFORM=MINGW $*
        ;;
esac

