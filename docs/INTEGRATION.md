# Integrating the library

The engine is one native library per platform. There is no service to run, no
initialisation call, no configuration file, and no background thread. Copy the
binary in, load it, call a function, free the string.

All examples below assume ABI 2. Check `hrt_abi_version()` at startup and refuse
to run against a different number — a newer library may accept a request whose
fields mean something else.

---

## Android

The library is a normal JNI-free shared object. It does not need a `System.load`
from Java: Dart, Rust or any other FFI caller opens it directly.

```
app/src/main/jniLibs/
├── arm64-v8a/libhrt_core.so
├── armeabi-v7a/libhrt_core.so
└── x86_64/libhrt_core.so
```

Gradle packages `jniLibs` automatically. If you build the library yourself,
`cargo ndk` writes exactly that layout:

```bash
cargo ndk -t arm64-v8a -t armeabi-v7a -t x86_64 \
    -o android/app/src/main/jniLibs build --release
```

**Loading.** `DynamicLibrary.open("libhrt_core.so")` — the loader finds it in the
APK's native library directory. Do not use an absolute path; the directory is
extracted and differs between installs.

**Page size.** Android 15 requires 16 KB-aligned shared objects on some devices.
NDK r28 produces them by default; with an older NDK add
`-C link-arg=-Wl,-z,max-page-size=16384` to `RUSTFLAGS`.

**ABI splits.** Use `android { splits { abi { … } } }` or an App Bundle. A
universal APK carries all three copies of the library (≈2 MB total).

**No permissions.** The library opens no sockets and no files. It needs nothing
in the manifest.

---

## iOS

iOS refuses to load a dylib that is not inside the signed bundle, so the core is
linked **statically** into the app executable.

```bash
rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios
cargo build --release --target aarch64-apple-ios          # device
cargo build --release --target aarch64-apple-ios-sim      # simulator
cargo build --release --target x86_64-apple-ios           # simulator, Intel
lipo -create \
  target/aarch64-apple-ios-sim/release/libhrt_core.a \
  target/x86_64-apple-ios/release/libhrt_core.a \
  -output target/ios-sim/libhrt_core.a
```

Link flags go in `ios/Flutter/Debug.xcconfig` and `Release.xcconfig` (or the
target's *Other Linker Flags*):

```
OTHER_LDFLAGS = $(inherited) -Wl,-export_dynamic -force_load $(SRCROOT)/rust/target/aarch64-apple-ios/release/libhrt_core.a
```

- `-force_load` — otherwise the linker drops every object from the archive,
  since nothing references the symbols at link time.
- `-export_dynamic` — keeps them in the dynamic symbol table, which is what
  `dlsym` reads.
- Do **not** strip the archive (`strip = true` in a release profile is fine for
  the machine code but `CARGO_PROFILE_RELEASE_STRIP=none` is safer when the
  symbols are resolved at run time).

**Loading.** `DynamicLibrary.process()` — the symbols come from the executable
itself, not from a separate image.

**Distribution.** An unsigned `.ipa` cannot be installed on a device. AltStore,
Sideloadly and Xcode all re-sign with a personal or developer certificate. If
you ship through the App Store, include the `ITSAppUsesNonExemptEncryption =
false` key: the library uses no cryptography.

**Simulator.** The same Dart/Objective-C code works; only the archive you link
changes.

---

## Windows

```
your-app/
├── your-app.exe
├── hrt_core.dll
└── flutter_windows.dll …
```

`cargo build --release` emits `hrt_core.dll` and `hrt_core.dll.lib` (the import
library, for C/C++ linkers). Put the DLL next to the executable; Windows looks
there before `PATH`.

CMake projects can copy it after the link:

```cmake
add_custom_command(TARGET your_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/rust/target/release/hrt_core.dll"
          "$<TARGET_FILE_DIR:your_app>")
```

- **Calling convention** is `cdecl`; declare it explicitly in C#:
  `[DllImport("hrt_core", CallingConvention = CallingConvention.Cdecl)]`.
- **Architecture** must match: an x64 process cannot load an x86 DLL.
- **Strings** are UTF-8. In .NET 5+ `Marshal.PtrToStringUTF8` reads them; on
  older frameworks use `Marshal.PtrToStringAnsi` and convert, or stay with the
  provided binding.
- **MSI/installer** must include the DLL as a file of the install directory, not
  as a system32 dependency.

---

## Linux and macOS

```bash
cargo build --release
# Linux:   target/release/libhrt_core.so
# macOS:   target/release/libhrt_core.dylib
```

- Prefer a relative rpath (`-Wl,-rpath,'$ORIGIN'` on Linux,
  `@loader_path` on macOS) over `LD_LIBRARY_PATH`, which is ignored for
  setuid binaries and easy to get wrong.
- macOS distribution outside the App Store needs signing and notarisation, and
  the dylib must be signed too.
- A universal macOS binary: build `x86_64-apple-darwin` and
  `aarch64-apple-darwin`, then `lipo -create`.

---

## Flutter

| Platform | How the core reaches the app |
|---|---|
| Android | `jniLibs/<abi>/libhrt_core.so`, opened by name |
| iOS | static archive linked into `Runner`, `DynamicLibrary.process()` |
| Windows | `hrt_core.dll` next to the exe |
| macOS | `libhrt_core.dylib` in `Contents/Frameworks`, opened by path |
| Linux | `libhrt_core.so` next to the bundle |

`bindings/dart/hrt_core.dart` selects the right one:

```dart
final core = HrtCore.open();          // platform switch is inside
final stats = core.stats({
  'events': events,
  'bodyWeightKg': 70,
  'nowH': nowHours,
  'startH': nowHours - 168,
  'endH': nowHours,
  'e2Target': {'low': 100, 'high': 200},
});
```

The call is synchronous. A single simulation of a year of doses takes a few
milliseconds; `steadyState` and `calibrate` re-simulate internally and can take
tens, so run those through `Isolate.run` if the UI is animating:

```dart
final report = await Isolate.run(() => core.steadyState({...}));
```

Sending the request as a `Map` across the isolate boundary is fine; the library
handle itself is not, so create the instance inside the isolate (opening the
same library twice is cheap and safe).

---

## Python

```python
from hrt_core import HrtCore, HrtError

core = HrtCore()            # HRT_CORE_LIB overrides the search
try:
    plan = core.plan_dose(
        schedule={"route": "injection", "ester": "EV", "doseMG": 4, "intervalH": 168},
        target={"low": 100, "high": 200},
        objective="trough",
        bodyWeightKg=70,
    )
except HrtError as exc:
    print(exc.code, exc.message)
```

- The binding is ctypes, so the GIL is released during the call: several threads
  can call at once.
- Freeing happens inside `_decode`, including on the error path; a leaked
  pointer is a bug worth reporting.
- `core.batch([...])` is the cheapest way to make many calls: one crossing, one
  JSON parse per call, a single Python-level call.

---

## C and C++

```bash
cc -Iinclude app.c -L. -lhrt_core -Wl,-rpath,'$ORIGIN' -o app
```

```cpp
#include "hrt_core.h"

std::string call(char *(*fn)(const char *), const std::string &json) {
    char *out = fn(json.c_str());
    std::string text = out ? out : "";
    hrt_free(out);
    return text;
}
```

Compile as C++ with `extern "C"` already in the header. Never free the pointer
with `free()`: the string was allocated by Rust's allocator, and only
`hrt_free` knows how to release it.

---

## Rules that apply everywhere

**Ownership.** Every returned pointer is a fresh allocation. Free it exactly
once, with `hrt_free`, including when you only wanted to log it. The pointer is
invalid after freeing; do not cache it.

**Threads.** Calls share no state. You may call from any thread, including
concurrently. There is no global initialisation and no finaliser.

**Errors are values.** A bad request returns `{"ok":false,…}`; it does not throw,
abort or write to stderr. Check `ok` before reading `result`.

**Time.** Everything is hours since the Unix epoch, UTC, as a float. Convert
local times yourself; the library has no timezone database. Durations are hours.

**Determinism.** Same input, same output, on every platform and every run. No
random numbers, no clock reads, no floating-point mode changes. That is what
makes snapshot tests possible.

**Versioning.** `hrt_abi_version()` changes only when a field changes meaning.
New optional fields and new functions do not bump it; renamed or reinterpreted
fields do. Pin the version you tested against.

**Performance.** A single dose simulated over 30 days is ~3 000 samples and
around 0.2 ms; 4 000 doses over ten years is ~100 000 samples and a few
milliseconds. Memory is proportional to the sample count: 32 bytes per sample
across the four arrays.
