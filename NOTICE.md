# Attribution and provenance

## 1. The model

The pharmacokinetic model implemented here is a port of the one in
**Oyama's HRT Tracker**, a browser application by Joseph Smirnova Oyama:

- Repository: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- Author: Joseph Smirnova Oyama (GitHub: [@SmirnovaOyama](https://github.com/SmirnovaOyama))
- Licence: MIT, © 2025 Joseph Smirnova Oyama

The original is a TypeScript application; this repository is a rewrite of its
calculation layer in Rust, with the arithmetic checked against the original for
a real dosing history. What came from upstream:

- the structure of the model: sequential first-order absorption, ester
  hydrolysis and elimination, with a fast and a slow fraction for biphasic
  esters;
- the rate constants, bioavailability fractions, volume-of-distribution
  constants, age–clearance slopes and sublingual tier table;
- the patch pairing rules (minimum wear, grace period, maximum overwear);
- the three calibration methods (`mipd`, `ekf`, `ou_kalman`) and their
  parameters;
- the JSON field names, including `doseMG`, so records and backups written by
  the original can be replayed without translation.

## 2. What changed here

- The model is written in Rust and exposed as a C ABI over JSON, so the same
  code serves Android, iOS, Windows, macOS, Linux and any language with an FFI.
- The analytic solution uses divided differences evaluated by three methods
  (direct, `sinh(z)/z`, Taylor series), so coincident rate constants stay finite
  without special-casing each combination.
- New: curve statistics, time in range, steady state, dose solving, reminder
  prediction, adherence, supply forecasting, unit conversion, a machine-readable
  catalogue, sublingual inversion, patch pairing output, plausibility warnings
  and parameter sensitivity.
- The simulation accepts an explicit window and step, and keeps the automatic
  peak-refinement grid only when the caller does not ask for one.

## 3. Third-party code

Rust dependencies are listed in `Cargo.toml` and pinned in `Cargo.lock`:

- `serde`, `serde_json` — MIT / Apache-2.0.

The Dart binding uses `package:ffi` (BSD-3-Clause); the Python binding uses only
the standard library; the C# binding uses `System.Text.Json`.

## 4. Licence

MIT — see [LICENSE](LICENSE). The upstream copyright notice is preserved there
and applies to the upstream-derived model; the rewrite and the added functions
are © 2026 Misaka hrt记录器 contributors.

## 5. A note on scope

This library computes what a model predicts given a dosing history. It is not a
medical device, it has not been reviewed by any regulator, and it does not
provide dosing advice. The constants are population values; the calibration
narrows them with a person's own labs but cannot validate them.
