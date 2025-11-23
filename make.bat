DEL config.h
IF NOT EXIST config.h COPY config.def.h config.h
zig cc ^
    -o dwm-win32.exe dwm-win32.c ^
    -DPROJECT_NAME='"dwm-win32"' -DPROJECT_VER='"0.1.0"' -DPROJECT_VER_MAJOR=0 -DPROJECT_VER_MINOR=1 -DPROJECT_VER_PATCH=0 ^
    -DNDEBUG -O2 -s ^
    -target x86_64-windows-msvc -std=c99 -pedantic -Wall