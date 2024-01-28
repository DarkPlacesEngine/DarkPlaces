rm build/darkplaces-emscripten.html
cd builddeps
rm d0_blind_id/bin/*
rm d0_blind_id/include/d0_blind_id/*
rm -rf d0_blind_id/lib/*
echo "x" > d0_blind_id/bin/BLINDIDBINHERE
echo "x" > d0_blind_id/include/d0_blind_id/BLINDIDHEADERSHERE
echo "x" > d0_blind_id/lib/BLINDIDLIBHERE
rm jpeg/include/*
rm jpeg/lib/*
echo "x" > jpeg/include/JPEGINCLUDESHERE
echo "x" > jpeg/lib/JPEGLIBHERE
rm gmp/include/*
rm gmp/lib/*
echo "x" > gmp/include/GMPINCLUDEHERE
echo "x" > gmp/lib/GMPLIBHERE
cd ../..
make clean
cd Darkplaces-Emscripten