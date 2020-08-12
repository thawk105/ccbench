# prepare for mimalloc
cd third_party/mimalloc
mkdir -p out/release
cd out/release
cmake ../..
make -j

cd ../
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -j

cd ../
mkdir secure
cd secure
cmake -DMI_SECURE=ON ../..
make -j

# prepare for tbb
cd ../../../tbb
make -j

cd ../../
