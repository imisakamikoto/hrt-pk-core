# Developing

## Prerequisites

| Tool | Version | Needed for |
|---|---|---|
| Rust | 1.75+ | everything |
| `cargo-ndk` | 4.x | Android (`cargo install cargo-ndk`) |
| Android NDK | r26+ | Android (`ANDROID_NDK_HOME`) |
| Xcode command line tools | 15+ | iOS/macOS |
| Visual Studio 2022 (C++ workload) | — | Windows |
| Python 3.9+ | — | the Python binding and its self-test |

The crate has two dependencies, `serde` and `serde_json`, both pure Rust.

## Layout

```
src/
├── lib.rs           crate root, re-exports, integration test
├── model.rs         vocabulary: doses, esters, routes, labs, params, targets
├── engine.rs        rate constants, analytic solution, patch pairing, simulation
├── calibration.rs   mipd / ekf / ou_kalman fits
├── stats.rs         peak, trough, AUC, time in range
├── planning.rs      steady state, dose solver, adherence, supply
├── reminders.rs     crossings of a lower bound
├── units.rs         concentration and ester-dose conversion
├── catalog.rs       the tables as data
├── safety.rs        plausibility warnings
├── analysis.rs      sensitivity to a parameter
└── api.rs           C ABI, JSON envelope, dispatch
include/hrt_core.h   the header a C caller sees
bindings/            Dart, Python, C#
examples/            C and Rust examples
scripts/             build-all, verify
docs/                API, model, integration, this file
```

Layer rule: `api.rs` may use anything; `engine`, `calibration`, `stats`,
`planning`, `reminders` depend on `model`; `safety` and `analysis` may use the
layers beneath them. No module reads the clock, the filesystem or the
environment — `env!("CARGO_PKG_VERSION")` at compile time is the only exception.

## Everyday commands

```bash
cargo test                      # 32 unit + 1 doc test
cargo test -- --nocapture       # if a test prints
cargo build --release
cargo fmt --check
cargo clippy --all-targets      # requires rustup component add clippy
```

The suite is small on purpose: each test pins one behaviour that would be silent
if it broke.

| Area | What the tests protect |
|---|---|
| `engine` | divided differences at coincident rates; plausible peak for oral E2 and weekly EV; patch pairing |
| `model` | the `doseMG` spelling the app actually sends, and that it round-trips |
| `calibration` | a doubled lab doubles the scale |
| `stats` | trapezoid, crossing split, peak time |
| `planning` | convergence, dose lands in range, coverage counted from the window |
| `reminders` | crossing found before it happens, hours below counted |
| `units` | pmol/L and nmol/L round-trip, valerate → estradiol equivalence |
| `safety` | ignored doses are reported; a clean record only mentions labs |
| `api` | error envelopes, catalog shape, batch, conversion |
| `lib` | end-to-end: schedule → steady state → dose solution |

## Building every target

```bash
bash scripts/build-all.sh
```

This runs, in order:

1. host `cargo build --release` (dylib/so/dll depending on the OS);
2. Android, if `ANDROID_NDK_HOME` is set — three ABIs into `dist/android/<abi>/`;
3. iOS, on macOS — device and simulator archives into `dist/ios/`;
4. Linux/macOS/Windows native copies into `dist/<platform>/`.

Every output lands in `dist/` with a predictable name, which is what the release
workflow uploads.

## Adding a function

1. **Decide whether the answer belongs in Rust at all.** If it is presentation,
   it belongs in the client. The library should answer questions about the
   curves, not lay them out.
2. Add the logic in the lowest module that can express it, with a unit test.
   Keep it pure: take `now_h` as an argument instead of reading the clock.
3. Add a handler in `api.rs` and register it in `dispatch`. Reuse
   `CommonRequest` for the fields every simulation needs.
4. Export it with the `exported!` macro, or write the `extern "C"` by hand if it
   needs no input (`hrt_version`).
5. Declare it in `include/hrt_core.h` with a comment that says what the result
   means, not just its type.
6. Add it to `capabilities()`.
7. Add a thin wrapper to each binding, and document the request in
   `docs/API.md`.
8. Add it to the "What it can answer" table in `README.md` (and the two
   translations).

Rules that keep the ABI predictable:

- Requests are objects with `#[serde(default)]` on everything optional, so old
  payloads keep parsing.
- Responses only ever **add** fields. Removing or repurposing one bumps
  `ABI_VERSION`.
- A `null` result means "nothing to say", not "error". Use the error envelope
  for malformed input.
- Never let a panic escape: `respond` wraps every call in `catch_unwind`.

## Versioning

| Change | Action |
|---|---|
| New function, new optional request field, new response field | minor version |
| Meaning of an existing field changes, function renamed or removed | major version **and** `ABI_VERSION` + 1 |
| Bug fix that changes a number | patch version, and say so in `CHANGELOG.md` |

`Cargo.toml` and `ABI_VERSION` are the two places to touch. `hrt_version()`
reads the crate version, `hrt_abi_version()` the constant.

## Releasing

```bash
# 1. bump Cargo.toml (+ ABI_VERSION if needed), update CHANGELOG.md
cargo test
bash scripts/build-all.sh

# 2. tag and push
git tag v2.0.1 && git push origin v2.0.1
```

The `build` workflow runs on every push and pull request; the `release` workflow
runs on a `v*` tag, rebuilds every target on its native runner, and attaches the
artifacts to a GitHub release. Nothing is published to crates.io automatically.

## CI

`.github/workflows/build.yml` has five jobs:

| Job | Runner | Produces |
|---|---|---|
| `linux` | ubuntu-latest | `.so`, plus `cargo test` and `cargo fmt --check` |
| `macos` | macos-14 | universal `.dylib` |
| `windows` | windows-latest | `.dll` + import library |
| `android` | ubuntu-latest | three `libhrt_core.so` |
| `apple` | macos-14 | iOS device `.a`, simulator universal `.a` |

Each job uploads an artifact named `hrt-core-<platform>`. The release job
downloads all of them, renames the files with the version, and creates the
release — so a broken platform fails the release instead of shipping a missing
binary.

## Style

- Doc comments explain *why*, not *what*; the code says what.
- Units and reference frames go in the doc comment of the field, not in its
  name only (`time_h`, not `time`).
- Public functions that return a number that could be zero for two different
  reasons return `Option` or an explicit flag; callers cannot guess.
- No `unwrap()` on anything derived from input. `expect()` is acceptable only
  for a static string that cannot be invalid.
- Prefer an explicit `match` over a chain of `if` for enums: adding a variant
  should break the build, not silently fall into a default.
