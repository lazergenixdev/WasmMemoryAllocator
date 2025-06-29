:: Compile with clang
clang -Wall -Wpedantic -O0 --target=wasm32 --no-standard-libraries -Wl,--allow-undefined -Wl,--export-all -Wl,--no-entry -o example.wasm main.c