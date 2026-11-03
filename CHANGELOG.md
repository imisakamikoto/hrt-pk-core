# Changelog

All notable changes to this library. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/), and `ABI_VERSION` follows the rules
in `docs/DEVELOPING.md`.

## Unreleased

<!-- New entries go here, newest first. See CONTRIBUTING.md for what counts as
     a notable change: a new or changed export, a change to the maths, a
     platform that starts or stops working — not a reformat. -->

## 2.2.1 — 2026-11-03

A payload fix and one arithmetic fix. `ABI_VERSION` stays at 2: no request
changed shape, and the response fields are the ones the documentation already
promised.

### Fixed

- **Milligram fields came out as `doseMg`, not `doseMG`.** `hrt_blockade`,
  `hrt_supply`, `hrt_plan_dose` and `hrt_compare` serialised the unit in
  camelCase while `docs/API.md` and every client spelled it in capitals. A
  client reading the documented name got `null` and fell back to zero, so a
  logged dose displayed as `0 mg`. The capitalised spelling is now what goes
  out; `…Mg` is still accepted when parsing.
- **`dailyMG` divided the window, not the record.** One 120 mg dose logged
  today was reported as 4 mg a day, which put a normal dose below the
  therapeutic band. The divisor is now the days the record actually covers —
  from the first in-window dose to now, floored at one day and capped at
  `windowDays`. Coverage already reports how empty the window is.

## 2.2.0 — 2026-11-03

Four new entry points, all additive: `ABI_VERSION` stays at 2 because no
existing request or response changed shape.

### Added

- **Regimen comparison** — `hrt_compare`: one to six schedules, each settled to
  steady state before it is read, reported in its own analyte's units with
  `steadiest`, `highestMean` and `lowestWeeklyMg` called out. `activeDailyMG`
  strips the ester's weight, which is the only cross-ester dose comparison that
  means anything.
- **Scenario analysis** — `hrt_what_if`: skip, double, halve, delay or stop, each
  run against the record as logged over the same window and reported as
  percentage deltas. Nothing at or before `nowH` is ever altered, because a
  what-if tool that rewrites the past can make any scenario look good.
- **Lab panel review** — `hrt_lab_review`: each reading compared against the
  range for its own analyte, molar units converted first, trends requiring two
  readings and 10% of movement, and a cadence check (`fresh`, `dueSoon`,
  `overdue`) against a configurable follow-up interval.
- **Body measurements** — `hrt_body`: bust, underbust and the cup size they
  imply. The difference is the measurement and the letter is a convention, so
  all four national systems are returned (eu, cn, uk, us) with the arithmetic
  each uses. Growth is the least-squares slope of the difference, not of the
  bust. No volume is estimated and no target is declared.

### Changed

- `docs/MODEL.md` gains sections on canrenone, on cup-size conventions, and a
  longer list of what the model refuses to compute.
- `docs/API.md` now documents `hrt_blockade`, `hrt_progress`, `hrt_compare`,
  `hrt_what_if`, `hrt_lab_review` and `hrt_body`.

## 2.1.0 — 2026-11-03

Anti-androgens become first-class, and the long view arrives.

### Added

- **Spironolactone** — modelled through canrenone, its active metabolite: a
  three-step chain (absorb → hepatic conversion → elimination) with
  `spiroFCanrenone = 0.52` (0.65 absorbed × 0.80 converted), a 19 h canrenone
  half-life at once- or twice-daily dosing, and `2.8 L/kg` of distribution. The
  curve is canrenone in ng/mL; the parent drug is not reported, because it is
  gone within hours while canrenone carries the effect. Four new `PkParams`
  fields, all `#[serde(default)]` so older clients keep working.
- **`hrt_blockade`** — daily milligram totals against per-compound bands
  (spironolactone 50/200/400, cyproterone 12.5/50/100, bicalutamide 25/50/150,
  medroxyprogesterone 2.5/10/20), coverage, the next dose due from the median
  interval, and — separately — the mechanism each compound acts through and
  whether a fall in testosterone is expected. Spironolactone and bicalutamide
  block the receptor and are not credited with lowering testosterone.
- **`hrt_progress`** — testosterone suppression and treatment effectiveness over
  a day, a month and a year. Suppression is the one-sided 95% Wilson lower bound
  of the share of labs under 50 ng/dL, never inferred from the presence of an
  anti-androgen. Effectiveness is `min(modelled time in range, logged coverage)`.

### Changed

- `Ester::SPIRO` is no longer "unmodelled"; it produces a curve on the oral
  route and is listed by `hrt_catalog` with a 19 h half-life.
- `hrt_safety` gains spironolactone-specific advice: the receptor-blockade note,
  a low-dose note under 50 mg/day, and a high-dose note above 200 mg/day.

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
