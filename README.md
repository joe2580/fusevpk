# FuseVPK
Mount a [Valve](https://www.valvesoftware.com/en/about) [VPK](https://developer.valvesoftware.com/wiki/VPK) file as a read only filesystem with [FUSE](https://github.com/libfuse/libfuse)

# Compilation
```bash
cmake .
make
```

# Usage
## Mount:
```
./fusevpk <VPK Location> <Mount location>
```

## Unmount:
```
fusermount -u <Mount location>
```
