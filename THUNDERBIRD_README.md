# Building
```
```bash
./scripts/build_thunderbird.sh --prefix /home/user/.local/opt/llvm@thunderbird --src ./ --jobs 12
```

# Compiling a Program
```
```bash
export LD_LIBRARY_PATH=/Users/rkabrick/.local/opt/llvm@thunderbird/lib:$LD_LIBRARY_PATH
export TBCLANG=/Users/rkabrick/.local/opt/llvm@thunderbird/bin/clang
$TBCLANG offload/test/offloading/offloading_success.c -fopenmp --offload-arch=thunderbird -S -o out.s
```


*NOTE*: Right now inside of ClangLinkerWrapper.cpp I have a hardcoded path to a path on my system. This will have to be changed if you want this to work. I have to change this to more portable likely with an environment variable or something.

