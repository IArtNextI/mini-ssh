cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 && cd build && make -j`nproc`
cp compile_commands.json ..
