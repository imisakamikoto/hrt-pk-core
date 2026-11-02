# Changelog

All notable changes to this library. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/), and `ABI_VERSION` follows the rules
in `docs/DEVELOPING.md`.

## 2.0.0 — 2026-11-03

The engine becomes a standalone library. Until now it existed only inside the
Flutter application, as five private Rust files with two entry points.

### Added

- **Curves and statistics** — `hrt_stats`: peak, trough, time-weighted mean,
  AUC, time in range, fluctuation, per hormone, over an arbitrary window.
- **Custom sampling** — `startH`, `endH` and `stepH` on `hrt_simulate` and
  `hrt_stats`, with the peak-refinement samples kept only for the automatic
  grid.
- **Planning** — `hrt_steady_state` (converges cycle by cycle, reports
  accumulation) and `hrt_plan_dose` (solves the dose that puts a trough, peak or
  mean inside a target range, using the linearity of the model rather than a
  search).
- **Reminders** — `hrt_reminders`: falling crossings of a lower bound, shifted
  earlier by a lead time, with de-duplication and the hours spent under the
  bound.
- **Adherence and supply** — `hrt_adherence` (coverage, on-time percentage,
  longest gap) and `hrt_supply` (runway at the planned rate).
- **Conversions** — `hrt_convert`: pg/mL ↔ pmol/L ↔ ng/dL ↔ nmol/L with the
  analyte's own molecular weight, and ester dose equivalence
  (2 mg EV = 1.53 mg E2).
- **Data** — `hrt_catalog`: esters, routes, extras keys, units, default targets,
  and an explicit note for every compound that is *not* modelled.
- **Helpers** — `hrt_sublingual` (hold time ↔ mucosal fraction, both directions),
  `hrt_patches` (application/removal pairing), `hrt_safety` (doses the model
  ignores, patch overwear, cyproterone dose, range conflicts),
  `hrt_sensitivity` (how far clearance/weight/dose errors move the curve).
- **Batching** — `hrt_batch`: up to 64 calls in one FFI crossing, failures
  inline.
- **Introspection** — `hrt_capabilities`, and `code` on every error envelope.
- **Packaging** — C header, Dart/Python/C# bindings, C and Rust examples, a
  `build-all.sh` script, and a CI matrix for Linux, macOS, Windows, Android and
  iOS.
- **Documentation** — `docs/API.md`, `docs/MODEL.md`, `docs/INTEGRATION.md`,
  `docs/DEVELOPING.md`.

### Changed

- Crate renamed to `hrt-pk-core`; the library keeps the name `hrt_core`, so
  `libhrt_core.so` and `hrt_core.dll` are unchanged and an existing app keeps
  working.
- ABI version 1 → 2. Existing `hrt_simulate` and `hrt_calibrate` requests and
  responses are unchanged; the new functions are additions.
- Panics are caught at every entry point and returned as
  `{"ok":false,"code":"panic"}` instead of being described only as
  `engine panic`.
- `doseMG` is accepted with its alternate spellings (`doseMg`, `dose_mg`) on
  input, and always written back as `doseMG`.

### Fixed

- Time in range is computed by intersecting the interpolated segment with the
  band. The previous per-sample comparison missed a segment that dipped through
  the range between two samples, and reported zero for a curve that crossed it.
- Steady-state search no longer reads an uninitialised cycle count when
  `maxCycles` is below 2.

## 1.0.0 — 2026-10-30

The version embedded in the Flutter application: `hrt_simulate`,
`hrt_calibrate`, `hrt_free`, `hrt_abi_version`, `hrt_version`. ABI 1.
