if ! type "emcc" > /dev/null; then
    printf "\nEmscripten Not installed. Please install Emscripten at https://emscripten.org/docs/getting_started/downloads.html \n"
    exit 1
fi

cd builddeps
./buildgmp.sh
./buildblindid.sh
./buildjpeg.sh 
cd ../..
emmake make sdl-release STRIP=:"$@" EXE_UNIXSDL="darkplaces-emscripten.html" DP_SSE=0 LIBM="" LIBZ="" LIB_CRYPTO_RIJNDAEL=../../../Darkplaces-Emscripten/builddeps/d0_blind_id/lib/libd0_rijndael.a LIB_CRYPTO_RIJNDAEL+="-sINITIAL_MEMORY=1500MB" EXE_UNIXSDL=darkplaces-emscripten.html LIB_CRYPTO=../../../Darkplaces-Emscripten/builddeps/d0_blind_id/lib/libd0_blind_id.a LIB_CRYPTO+=" ../../../Darkplaces-Emscripten/builddeps/gmp/lib/libgmp.a" LIBM="" LIBZ="" LIB_JPEG=../../../Darkplaces-Emscripten/builddeps/jpeg/lib/libjpeg.a CFLAGS_EXTRA="-DNOSUPPORTIPV6 -sUSE_LIBPNG -sUSE_SDL=2 -DUSE_GLES2 -DLINK_TO_ZLIB -sUSE_ZLIB=1 -DLINK_TO_JPEG -I../../../Darkplaces-Emscripten/builddeps/d0_blind_id/include -I../../../Darkplaces-Emscripten/builddeps/jpeg/include" LDFLAGS_EXTRA="-LDarkplaces-Emscripten/builddeps/d0_blind_id/lib/ -Wl,-rpath,Darkplaces-Emscripten/builddeps/d0_blind_id/lib/ -LDarkplaces-Emscripten/builddeps/gmp/lib -Wl,-rpath,Darkplaces-Emscripten/builddeps/gmp/lib -LDarkplaces-Emscripten/builddeps/jpeg/lib/ -Wl,-rpath,Darkplaces-Emscripten/builddeps/jpeg/lib/ -sUSE_SDL=2 -sUSE_ZLIB=1 -DUSE_GLES2 -sINITIAL_MEMORY=1500MB -sMAXIMUM_MEMORY=4GB -sSINGLE_FILE -sFULL_ES2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH --pre-js ../../../Darkplaces-Emscripten/prejs.js -lidbfs.js -sEXPORTED_RUNTIME_METHODS='callMain' --shell-file=../../../Darkplaces-Emscripten/shell.htm "
mkdir Darkplaces-Emscripten/build
mv darkplaces-emscripten.html Darkplaces-Emscripten/build/darkplaces-emscripten.html