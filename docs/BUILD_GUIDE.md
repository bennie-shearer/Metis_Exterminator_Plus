# Build Guide - Metis Exterminator Plus

**Version 2.3.0**

---

## Prerequisites

### Windows (Primary)
- CLion 2024+ with bundled MinGW-w64 GCC 13
- CMake 3.20+ (bundled with CLion)
- Ninja (bundled with CLion)
- No additional installs required (all libraries bundled)

### Linux
- GCC 13+ or Clang 16+: `sudo apt install gcc-13 g++-13` (Ubuntu 22.04+)
- CMake 3.20+: `sudo apt install cmake`
- Ninja: `sudo apt install ninja-build`
- OpenSSL dev (optional, prebuilt included): `sudo apt install libssl-dev`

### macOS
- Xcode Command Line Tools: `xcode-select --install`
- CMake 3.20+: `brew install cmake`
- Ninja: `brew install ninja`

---

## Build Steps

### CLion (Windows - recommended)

1. Open the project folder in CLion
2. CLion detects `CMakeLists.txt` automatically
3. Select the MinGW-w64 toolchain in Settings > Build > Toolchains
4. Click Build > Build Project (Ctrl+F9)
5. Executable: `cmake-build-debug/Metis_Exterminator_Plus.exe`

### Command Line (all platforms)

```bash
# Configure
cmake -S . -B build -G Ninja

# Build
cmake --build build --parallel

# Run (Windows)
build\Metis_Exterminator_Plus.exe

# Run (Linux/macOS)
./build/Metis_Exterminator_Plus
```

### Release Build

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
```

---

## OpenSSL Prebuilts

The project ships with prebuilt OpenSSL static libraries:

```
openssl-prebuilt/
  windows/lib/    libssl.a  libcrypto.a
  linux/lib/      libssl.a  libcrypto.a   (symlinks or copies from system)
  macos/lib/      libssl.a  libcrypto.a   (symlinks or copies from system)
```

CMake selects the correct library directory automatically based on the target platform.

To rebuild from source (optional):
```bash
# Linux/macOS
git clone https://github.com/openssl/openssl
cd openssl && ./Configure --prefix=$(pwd)/out && make && make install
```

---

## TLS Certificate

A self-signed development certificate is included in `tls/`. For production:

```bash
# Regenerate with OpenSSL (uses san.cfg for SAN extension)
openssl req -x509 -newkey rsa:4096 -keyout tls/server.key \
    -out tls/server.crt -days 3650 -nodes \
    -config tls/san.cfg
```

---

## CMake Notes

- `CMAKE_SUPPRESS_REGENERATION true` prevents dirty-manifest regeneration loops on Windows
- SQLite is compiled as a C source file with `SQLITE_THREADSAFE=1` and `SQLITE_ENABLE_FTS5`
- bcrypt requires the `crypt_blowfish/` subfolder alongside `bcrypt.c`
- All third-party sources are included directly; no `find_package` calls

---

## Verifying the Build

After a successful build, the output directory contains:

```
Metis_Exterminator_Plus[.exe]
web/              (copied from project web/)
config/           (copied from project config/)
tls/              (copied from project tls/)
```

Run the executable and open `http://2.3.0.1:9100` in Chrome.
