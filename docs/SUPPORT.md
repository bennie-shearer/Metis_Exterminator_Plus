# Support - Metis Exterminator Plus

**Version 3.1.0**

---

## Self-Help Resources

1. **HOWTO.md** - Step-by-step guides for common tasks
2. **CONFIGURATION.md** - Complete config.pson reference
3. **API.md** - Full REST endpoint reference
4. **BUILD_GUIDE.md** - Build instructions for all platforms

---

## Common Issues

### Server does not start

- Check that port 9100 is not already in use: `netstat -an | findstr 9100` (Windows) or `lsof -i:9100` (Linux/macOS)
- Check the config.pson path: the server looks for `config/config.pson` relative to the executable
- Verify the `data/` directory is writable

### Browser shows "Cannot connect"

- Confirm the server is running and printed the startup message
- Open `http://2.3.0.1:9100` (not https unless tls_enabled = true)
- Check Windows Firewall if accessing from another machine

### Data missing after restart

- In . Clearing browser data wipes it. Use Export before clearing.
- In server mode: check that `data/metis_exterminator.db` exists and was not deleted

### Build fails on Windows

- Confirm MinGW-w64 GCC 13 is selected as the CLion toolchain
- Ensure `CMAKE_SUPPRESS_REGENERATION true` is in CMakeLists.txt
- Check that `openssl-prebuilt/windows/lib/libssl.a` and `libcrypto.a` are present

### Build fails: bcrypt cannot find crypt_blowfish

- The `third_party/bcrypt/crypt_blowfish/` subdirectory is required
- This directory contains the OpenBSD Blowfish implementation that bcrypt.c includes
- It is not included in the repository; obtain from https://www.openwall.com/crypt/

### Invoice numbers restart from 1000

- The `next_invoice_number` in config.pson is only the starting point for the first run
- After the first invoice is created, the counter is tracked in memory and saved to the flat-file stores
- After a clean restart, the counter resumes from the last issued number (read from the data files)

---

## Reporting Issues

This is an MIT-licensed open source project. Issues and contributions are welcome via the project repository.

When reporting a bug, please include:
1. Operating system and version
2. Compiler version (GCC/Clang/MSVC)
3. CMake and Ninja version
4. The full server startup output
5. The browser console error (F12 > Console)
6. Steps to reproduce

---

## Acknowledgments

Metis Exterminator Plus was built with:
- CLion by JetBrains s.r.o. (https://www.jetbrains.com/clion/)
- Claude by Anthropic PBC (https://www.anthropic.com/)
- SQLite (https://sqlite.org/)
- OpenSSL Project (https://www.openssl.org/)
- bcrypt wrapper by Ricardo Garcia (CC0)
- nlohmann/json by Niels Lohmann (MIT)
