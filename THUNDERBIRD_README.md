# Building

````
```bash
./scripts/build_thunderbird.sh --prefix $HOME/.local/opt/llvm@thunderbird --src ./ --jobs 12
````

# Compiling a Program

````
```bash
export LD_LIBRARY_PATH=$HOME/.local/opt/llvm@thunderbird/lib:$LD_LIBRARY_PATH
export TBCLANG=$HOME/.local/opt/llvm@thunderbird/bin/clang
$TBCLANG offload/test/offloading/offloading_success.c -fopenmp --offload-arch=thunderbird -o offloading_success 
````

````
NOTE: May need to add `-I$HOME/.local/opt/llvm@thunderbird/include` to the compilation script if `omp.h` is missing  
