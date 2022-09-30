### Stack realization

### Methods of protection
1. Canary protection (DUNGEON_MASTER)
2. Poisoning after free & all unused array space poisoning (KSP)
3. Hash protection: data and struct itself (HASH)
4. Memory protection (MEMORY). Allocates data and copy of itself with mmap with RO access, using mprotect to change any value. Works only on linux

1. To compile 
```bash
make
```
2. To run tests
```bash
make run
```

3. To generate documentation
```bash
doxygen
```
