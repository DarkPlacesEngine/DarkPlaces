if ! type "emcc" > /dev/null; then
    printf "\nEmscripten Not installed. Please install Emscripten at https://emscripten.org/docs/getting_started/downloads.html \n"
    exit 1
fi
#m4 and libtool are required to compile. 

wget https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz
tar -xvf gmp-6.3.0.tar.xz
cd gmp-6.3.0
emconfigure ./configure --disable-assembly --host none
emmake make
cp .libs/libgmp.a ../gmp/lib
cp gmp.h ../gmp/include
cd ..
rm -rf gmp-6.3.0
rm gmp-6.3.0.tar.xz
