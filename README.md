# AI Caro

Implementing m,n,k-game

See [main.cc](main.cc) for n, m, k game modification

### Bots
- [x] Alpha-beta pruning
- [x] Alpha-beta pruning with YBWC
- [ ] MCTS
- [ ] Ant search tree 

### Requirements

- A C++20 compiler
- `xmake` 3.0.0 or later

### Build

#### 1. Configure using `xmake`:

- MSVC
```
xmake f --toolchain=msvc -m [release|debug]
```

- Windows: GCC or Clang on MinGW/MSYS2
```
xmake f -p mingw --toolchain=[gcc|clang] -m [release|debug]
```

- Windows: If you wish to use Clang with MSVC ABI, remove `mingw` platform
```
xmake f --toolchain=clang -m [release|debug]
```

- Linux/Mac
```
xmake f --toolchain=[gcc|clang] -m [release|debug]
```

#### 2. Build
```
xmake build ai-caro
```

- Example on Windows using MinGW Clang, release build

```
xmake f -p mingw --toolchain=clang -m release
xmake build ai-caro
```

The resulting executable is placed in `bin/`.

#### Extra: VSCode IntelliSense
Run command to create `compile-commands.json` file. Also need to configure as described [here](https://code.visualstudio.com/docs/cpp/configure-intellisense#_compilecommandsjson-file)
```
xmake project -k compile_commands
```