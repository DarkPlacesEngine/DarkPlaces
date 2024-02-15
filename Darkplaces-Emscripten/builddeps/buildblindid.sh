if ! type "emcc" > /dev/null; then
    printf "\nEmscripten Not installed. Please install Emscripten at https://emscripten.org/docs/getting_started/downloads.html \n"
    exit 1
fi
#Make sure to run buildgmp.sh first

git clone https://github.com/divVerent/d0_blind_id .blindid
cd .blindid
chmod +x autogen.sh
emconfigure ./autogen.sh

LIBS=../gmp/lib/libgmp.a emconfigure ./configure --libdir=`pwd`/../ld0_blind_id/lib --includedir=`pwd`/../ld0_blind_id/include --oldincludedir=`pwd`/../ld0_blind_id/include --bindir=`pwd`/../ld0_blind_id/bin --host none
cp ../gmp/lib/libgmp.a . 
cp ../gmp/include/gmp.h .
emmake make
emmake make install
cd ..
rm -rf .blindid