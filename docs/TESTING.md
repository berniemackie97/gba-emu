# Testing

Unit and integration tests use GoogleTest and CTest discovery. Frontend
lifecycle scenarios run the real SDL executable under the dummy video driver.

## Run the complete suite

Linux and macOS:

```sh
cmake --build --preset default --parallel
ctest --preset default --output-on-failure
```

Windows (Visual Studio 2022):

```powershell
cmake --build --preset windows --config Debug --parallel
ctest --preset windows --build-config Debug --output-on-failure
```

## Asset safety regressions

`bios_load_test` and `cart_map_test` use generated binary fixtures, so
they do not require copyrighted BIOS or game data. They verify exact
BIOS sizing, the 192-byte minimum for file-backed cartridges, the
32 MiB upper bound, regular-file requirements, structured failure
codes, preserved loaded state and successful retry. Tiny in-memory
instruction images have a separate test contract.

`core_smoke_test` exercises reset, generated cartridge load, instruction
execution and reset. `Frontend.startup`, `Frontend.invalid-video` and
`Frontend.invalid-arguments` are CTest scenarios for process lifetime and
exit behavior.

Passing these tests does not imply complete ARM7TDMI, memory-map or
commercial game compatibility.
