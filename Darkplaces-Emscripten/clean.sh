rm build/darkplaces-emscripten.html
cd builddeps
rm ld0_blind_id/bin/*
rm ld0_blind_id/include/d0_blind_id/*
rm -rf ld0_blind_id/lib/*
echo "x" > ld0_blind_id/bin/.gitkeep
echo "x" > ld0_blind_id/include/d0_blind_id/.gitkeep
echo "x" > ld0_blind_id/lib/.gitkeep
rm jpeg/include/*
rm jpeg/lib/*
echo "x" > jpeg/include/.gitkeep
echo "x" > jpeg/lib/.gitkeep
rm gmp/include/*
rm gmp/lib/*
echo "x" > gmp/include/.gitkeep
echo "x" > gmp/lib/.gitkeep
cd ../..
make clean
cd Darkplaces-Emscripten