if ! type "emcc" > /dev/null; then
    printf "\nEmscripten Not installed. Please install Emscripten at https://emscripten.org/docs/getting_started/downloads.html \n"
    exit 1
fi
#doesn't work on codespaces but works on a normal computer
git clone https://github.com/libjpeg-turbo/libjpeg-turbo .jpeg
cd .jpeg
emcmake cmake -G'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release -DWITH_SIMD=0 .
emmake make
cp jconfig.h ../jpeg/include
cp jerror.h ../jpeg/include
cp jmorecfg.h ../jpeg/include
cp jpeglib.h ../jpeg/include
cp libjpeg.a ../jpeg/lib
cd ..
rm -rf .jpeg