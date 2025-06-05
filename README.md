CPP-Drivechain
=====================================
C++ implementation of drivechain that is compatible with the CUSF Enforcer (https://github.com/LayerTwo-Labs/bip300301_enforcer)

Getting Started:
----------------
Build with these options / use build-unix.sh:
```
cmake -B build -DBUILD_GUI=ON -DBUILD_BENCH=OFF -DBUILD_FUZZ_BINARY=OFF -DBUILD_GUI_TESTS=OFF -DBUILD_TESTS=OFF
cmake --build build -j $(nproc)
```

Default data directories:
```
// Windows: C:\Users\Username\AppData\Local\drivechaincpp
// macOS: ~/Library/Application Support/drivechaincpp
// Unix-like: ~/.drivechaincpp
```

Default config file name: `drivechaincpp.conf`


Based on Bitcoin Core:
---------------------

https://bitcoincore.org

Bitcoin Core connects to the Bitcoin peer-to-peer network to download and fully
validate blocks and transactions. It also includes a wallet and graphical user
interface, which can be optionally built.

Further information about Bitcoin Core is available in the [doc folder](/doc).

License
-------

Bitcoin Core and cpp-drivechain are released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.
