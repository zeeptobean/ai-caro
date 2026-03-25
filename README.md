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

1. Configure using `xmake`:

```
xmake f --toolchain=gcc -m [release|debug]
```

Toolchain could be `msvc` for Visual C++, `clang` or `llvm`. On Windows `msvc` or `gcc` is prefered (even with MinGW)

2. Build
```
xmake build ai-caro
```

- Example on Windows using Visual C++, release build

```
xmake f --toolchain=msvc -m release
xmake build ai-caro
```

The resulting executable is placed in `bin/`.
