cmake -B build
cmake --build build
pushd build
ctest -C Debug --output-on-failure
popd
