### Stack realization

### Methods of protection
1. Canary protection (DUNGEON_MASTER)
2. Poisoning after free & all unused array space poisoning (KSP)
3. Hash protection: data and struct itself (HASH)
4. Memory protection (MEMORY). Allocates data and copy of itself with mmap with RO access, using mprotect to change any value. Works only on linux

### How to use
1. Compile tests binary (bin/stack)
```bash
make
```

2. Run tests
```bash
make run
```

3. Generate documentation
```bash
doxygen
```
It will create docs/html/index.html, that you can open with your browser.