# API reference — ABI 2

Every exported function has the same shape.

```
char *hrt_<name>(const char *request_json);
```

- `request_json` is a UTF-8 JSON object. `NULL` is an error; `{}` is an empty
  request.
- The return value is a UTF-8 JSON string owned by the library. Release it with
  `hrt_free`. It is valid until freed, and never aliases another result.
- Success: `{"ok": true, "result": …}`
- Failure: `{"ok": false, "code": "badRequest", "error": "human readable"}`

Calls are pure: no shared mutable state, no locking, safe from several threads
at once, and safe to call before any initialisation (there is none).

## Error codes

| `code` | Meaning |
|---|---|
| `badRequest` | The JSON parsed but a field was missing or the wrong type. `error` names the field. |
| `unknownFunction` | Only from `hrt_batch` and `hrt_capabilities` callers: no such function name. |
| `nullInput` | The pointer was `NULL`. |
| `notUtf8` | The input bytes were not valid UTF-8. |
| `panic` | An internal assertion failed. The library survived; the request did not. |
| `encode` | The response could not be serialised (should not happen). |
| `nul` | A response string contained a NUL byte (should not happen). |

An empty `result` is `null`, not an error: "there is nothing to simulate" is a
legitimate answer.

---

## Conventions

| Quantity | Unit |
|---|---|
| Concentration, estradiol | pg/mL |
| Concentration, testosterone | ng/dL |
| Concentration, cyproterone acetate | ng/mL |
| Time (`timeH`, `nowH`, `startH`, `endH`, `atH`, …) | hours since the Unix epoch, UTC |
| Durations (`intervalH`, `horizonH`, `leadH`, `halfLifeHours`) | hours |
| Dose (`doseMG`) | milligrams of the **logged ester**, not of the active hormone |
| Body weight | kilograms |
| Age | derived from `birthYear` (mid-year), not passed directly |

Concentrations are linear in dose and inversely proportional to body weight;
nothing else is normalised.

---

## Shared types

### `DoseEvent`

```json
{
  "id": "6e9c58b7",
  "route": "injection",
  "timeH": 498220.83,
  "doseMG": 4,
  "ester": "EV",
  "extras": {"releaseRateUGPerDay": 100},
  "updatedAt": 1793594989627
}
```

| Field | Type | Notes |
|---|---|---|
| `id` | string | Any stable identifier. Used only for patch output and duplicate detection. |
| `route` | enum | `sublingual`, `injection`, `patchApply`, `patchRemove`, `gel`, `oral`. |
| `timeH` | number | Hours since the epoch. |
| `doseMG` | number | Also accepted as `doseMg` / `dose_mg`. Serialised back as `doseMG`. |
| `ester` | enum | `E2`, `EB`, `EV`, `EC`, `EN`, `EU`, `CPA`, `SPIRO`, `BICA`, `MPA`, `LEUP`, `TRIP`, `RLX`, `T`, `TC`, `TE`, `TU`. |
| `extras` | object | Route-specific numbers, below. |
| `updatedAt` | integer | Optional; ignored by the engine. |

`extras` keys:

| Key | Route | Meaning |
|---|---|---|
| `sublingualTier` | sublingual | 0 quick (2 min), 1 casual (5), 2 standard (10), 3 strict (15). Default 2. |
| `sublingualTheta` | sublingual | Mucosal fraction directly, 0–1. Overrides the tier. |
| `gelSite` | gel | 0 arm, 1 thigh, 2 scrotal. Default 0. |
| `releaseRateUGPerDay` | patchApply | Labelled flux. Switches the patch to a zero-order input. |
| `patchWearH` | patchApply | Planned wear time, used to pair the removal. |
| `concentrationMGmL`, `areaCM2` | gel, patch | Recorded by clients; not read by the model. |

### `LabResult`

```json
{"id": "l1", "timeH": 498000, "concValue": 148.2, "unit": "pg/ml", "assay": "LC-MS/MS"}
```

`unit` is one of `pg/ml`, `pmol/l`, `ng/dl`, `nmol/l`. Molar units are converted
with the analyte's own molecular weight by the calibration and stats code, so a
testosterone result in nmol/L is not fed through estradiol's factor.

### `PkParams`

Every field is optional on the wire; omitted fields take the default. Values are
clamped to the range in the last column, so an imported backup cannot divide by
zero or turn decay into growth.

| Field | Default | Range |
|---|---|---|
| `e2KClear` | 0.41 | 0.001–5 |
| `e2KClearInj` | 0.041 | 0.001–1 |
| `e2FfEb` | 0.1092 | 0–1 |
| `e2FfEv` | 0.0623 | 0–1 |
| `e2FfEc` | 0.1173 | 0–1 |
| `e2FfEn` | 0.12 | 0–1 |
| `e2FfEu` | 0.04 | 0–1 |
| `e2OralBio` | 0.03 | 0–1 |
| `e2SlQuick` | 0.01 | 0–1 |
| `e2SlCasual` | 0.04 | 0–1 |
| `e2SlStandard` | 0.11 | 0–1 |
| `e2SlStrict` | 0.18 | 0–1 |
| `e2GelArm` | 0.05 | 0–1 |
| `e2GelThigh` | 0.05 | 0–1 |
| `e2GelScrotal` | 0.25 | 0–1 |
| `tKClear` | 0.5 | 0.001–5 |
| `tKClearInj` | 0.035 | 0.001–1 |
| `tFfTc` | 0.043 | 0–1 |
| `tFfTe` | 0.043 | 0–1 |
| `tFfTu` | 0.043 | 0–1 |
| `tGelArm` | 0.1 | 0–1 |
| `tGelThigh` | 0.1 | 0–1 |
| `tGelScrotal` | 0.5 | 0–1 |

### `TargetRange`

```json
{"low": 100, "high": 200}
```

`low` must be positive and below `high`; a reversed pair is normalised, a
non-positive or infinite one is treated as "no target".

### `Schedule`

A repeating dose, used by `steadyState`, `planDose` and `supply`.

```json
{"route": "injection", "ester": "EV", "doseMG": 4, "intervalH": 168, "extras": {}, "startH": 0}
```

---

## Functions

### `hrt_simulate`

Concentration curves for a set of doses.

**Request**

| Field | Type | Default | Notes |
|---|---|---|---|
| `events` | `DoseEvent[]` | required | Any order; sorted internally. |
| `bodyWeightKg` | number | required | Must be > 0, otherwise `result` is `null`. |
| `nowH` | number | required | Anchors the default window and is always sampled. |
| `birthYear` | number | none | Enables the age–clearance correction. |
| `params` | `PkParams` | defaults | |
| `clearanceMult` | number | 1 | Clamped to 0.1–10. |
| `startH`, `endH` | number | automatic | Window override. |
| `stepH` | number | automatic | Uniform output spacing; disables the extra peak samples. |
| `includeStats` | bool | false | Adds a `stats` member next to `result`. |
| `e2Target`, `tTarget` | `TargetRange` | none | Only used to compute time-in-range. |

The automatic grid spans 24 h before the first dose to two weeks after the last
(or a day past `nowH`), with 2 000–100 000 samples plus extra samples at
0.25/0.5/1/2/4/6/8/12/24/48 h after each dose so a fast peak is not missed.

**Result** — `SimulationResult`, or `null`.

```json
{
  "timeH": [0, 0.25, …],
  "e2PgMl": [0, 0.0, …],
  "cpaNgMl": [0, 0, …],
  "tNgDl": [0, 0, …],
  "auc": 12345.6
}
```

`auc` is the trapezoidal area under the **estradiol** curve over the whole
window, in pg/mL·h. The three arrays are always the same length as `timeH`.

---

### `hrt_stats`

Peak, trough, mean, AUC and time-in-range, per curve, over a window.

**Request** — the same fields as `hrt_simulate` (no `includeStats`).

**Result** — `SimulationStats` or `null`.

```json
{
  "e2": {
    "n": 480, "peak": 412.6, "peakH": 498100.0,
    "trough": 88.4, "troughH": 498580.0,
    "mean": 190.2, "auc": 91296.0,
    "timeInRangeH": 120.5, "timeInRangePct": 71.7,
    "fluctuationPct": 366.7, "spanH": 168.0
  },
  "t": null, "cpa": null,
  "windowStartH": 498000.0, "windowEndH": 498168.0
}
```

A curve whose peak is zero is reported as `null` rather than as a row of zeros.
`mean` is time-weighted (`auc / span`), not the arithmetic mean of the samples.
`timeInRange` is computed by solving the linear segment against the two bounds,
so it is independent of the sample spacing.

---

### `hrt_calibrate`

Fit the model to measured levels.

**Request**

| Field | Type | Default | Notes |
|---|---|---|---|
| `events` | `DoseEvent[]` | required | |
| `labs` | `LabResult[]` | required | Labs with no modelled level at their time are dropped. |
| `bodyWeightKg` | number | required | |
| `nowH` | number | required | |
| `method` | string | `mipd` | `mipd`, `ekf`, `ou_kalman`, `off`. |
| `historyMode` | string | `retrospective` | `forward` produces one correction per lab; `retrospective` one for the present. |
| `birthYear`, `params` | | none | |

**Result**

```json
{
  "method": "mipd", "scale": 1.21, "clearanceMult": 0.94,
  "halfLifeDeltaPct": 6.4, "n": 3, "fitErrPct": 8.1,
  "points": [{"timeH":…, "obs":…, "model":…, "ratio":…, "hormone":"e2"}],
  "segments": [{"timeH": 0, "factor": 1.21}]
}
```

- `scale` multiplies the curve; `clearanceMult` is the personal clearance
  multiplier to feed back into `hrt_simulate`.
- With fewer than two labs, or a span under 48 h, clearance and amplitude cannot
  be separated, so the fit degrades to the geometric mean of the observed
  ratios and `clearanceMult` is 1.
- `fitErrPct` is the RMS relative error of the fitted points, not a confidence
  interval.

---

### `hrt_steady_state`

Where a repeated schedule settles.

**Request**

| Field | Type | Default | Notes |
|---|---|---|---|
| `schedule` | `Schedule` | required | |
| `bodyWeightKg` | number | required | |
| `birthYear`, `params` | | none | |
| `maxCycles` | integer | 128 | Caps the search; convergence usually happens long before. |
| `e2Target`, `tTarget` | `TargetRange` | none | Enables `timeInRangePct`. |
| `includeSeries` | bool | false | Returns the last cycle's samples too. |

**Result**

```json
{
  "cycles": 12, "stable": true, "accumulationRatio": 2.4, "intervalH": 168,
  "cycle": {"e2": {…}, "t": null, "cpa": null, "windowStartH": …, "windowEndH": …},
  "series": null
}
```

`cycles` is how many doses were simulated before the last cycle was read — the
run stops when two consecutive cycles agree to within 1% in peak and trough.
`stable: false` means `maxCycles` was reached first; the numbers are then the
last cycle, not a plateau. `accumulationRatio` is the steady peak over the first
cycle's peak.

---

### `hrt_plan_dose`

The dose that places a chosen statistic inside a target range.

**Request**

| Field | Type | Default | Notes |
|---|---|---|---|
| `schedule` | `Schedule` | required | `doseMG` is ignored; the solver replaces it. |
| `target` | `TargetRange` | required | |
| `objective` | string | `trough` | `trough`, `peak` or `mean`. |
| `bodyWeightKg` | number | required | |
| `birthYear`, `params`, `maxCycles` | | | |

**Result**

```json
{
  "doseMG": 5.4, "objective": "trough",
  "achievedPeak": 415.4, "achievedTrough": 104.3, "achievedMean": 266.5,
  "inRange": false, "feasible": false, "cycles": 6,
  "note": "Placing the trough in range pushes the peak over the ceiling; …"
}
```

Concentration is linear in dose, so the solver simulates 1 mg and divides. The
suggested dose is then simulated again, and the numbers returned are from that
second run — not extrapolated. `inRange` is the honest answer to "does this
satisfy both bounds"; when it is false, `note` says which bound broke and why.

Some inputs have no solution: a weekly injection of a fast ester may swing wider
than a narrow range can hold. The solver returns the conflict instead of a dose
that merely looks plausible.

A patch with `releaseRateUGPerDay` delivers a fixed flux independent of the
logged amount, so the dose cannot be solved; the result carries
`feasible: false` and a note saying to change the patch, not the milligrams.

---

### `hrt_reminders`

When the level falls through a lower bound.

**Request**

| Field | Type | Default | Notes |
|---|---|---|---|
| `events`, `bodyWeightKg`, `nowH` | | required | |
| `low` | number | required | Lower bound, in the model's unit. |
| `horizonH` | number | 336 (14 days) | |
| `leadH` | number | 12 | Fire this long before the crossing. |
| `minGapH` | number | 6 | Suppress a reminder closer than this to the previous one. |
| `birthYear`, `params`, `clearanceMult` | | | |

**Result**

```json
{
  "reminders": [
    {"atH": 498560.0, "crossingH": 498572.0, "level": 100.0,
     "hoursUntil": 560.0, "leadH": 12.0}
  ],
  "hoursBelow": 18.5, "horizonH": 336.0, "low": 100.0
}
```

Only **falling** crossings are reported. A reminder whose lead time would place
it before `nowH` is clamped to `nowH`. `hoursBelow` is the time the curve spends
under `low` across the whole horizon — what happens if nothing is taken.

---

### `hrt_adherence`

How a log compares with a fixed interval.

**Request** — `events`, `intervalH`, `fromH`, `toH`.

**Result**

```json
{
  "expected": 5, "taken": 3, "coveragePct": 60.0, "onTimePct": 100.0,
  "extraDoses": 0, "longestGapH": 336.0, "longestGapStartH": 212.0,
  "gaps": [[212.0, 336.0]]
}
```

`expected` is counted from the window, not from the log, so someone who stopped
logging shows up as missing doses instead of a perfect record. A dose counts as
on time when it lands within a quarter of an interval of its slot. `coveragePct`
is capped at 100; a surplus is reported in `extraDoses`.

---

### `hrt_supply`

Runway at the planned rate.

**Request** — `{"onHandMG": 30, "schedule": {…}, "asOfH": 498000}`.

**Result**

```json
{"onHandMG":30.0,"dailyMG":0.571,"weeklyMG":4.0,"dosesRemaining":7,"daysRemaining":52.5,"runOutH":499260.0}
```

---

### `hrt_convert`

Two unrelated conversions under one name.

**Concentration** (default `kind`)

```json
{"kind":"concentration","value":150,"from":"pg/ml","to":"pmol/l","analyte":"e2"}
→ {"value":550.70,"from":"pg/ml","to":"pmol/l","analyte":"e2"}
```

`analyte` is `e2` or `t`; it selects the molecular weight. It defaults to what
the unit implies (`ng/dl`, `nmol/l` → testosterone; otherwise estradiol).

**Dose equivalence**

```json
{"kind":"dose","doseMG":2,"fromEster":"EV","toEster":"E2"}
→ {"doseMG":2.0,"activeMG":1.53,"equivalentMG":1.53,"fromEster":"EV","toEster":"E2"}
```

`activeMG` is the amount of active hormone in the logged ester; `equivalentMG`
is the amount of `toEster` that would carry the same. Both use molecular weight
ratios and ignore differences in absorption.

**Active amount only** — `{"kind":"active","doseMG":2,"fromEster":"EV"}`.

---

### `hrt_catalog`

Everything the engine accepts, as data. No request fields.

```json
{
  "version": "2.2.0", "abi": 2,
  "esters": [{"code":"EV","name":"Estradiol valerate","molecularWeight":356.5,
              "activeFactor":0.764,"hormone":"e2","modelled":true,
              "routes":["injection","oral","sublingual"],
              "halfLifeHours":120.0,"note":null}],
  "routes": [{"code":"injection","name":"Injection","description":"…",
              "extras":[],"isDose":true}],
  "units": [{"code":"pg/ml","analyte":"e2","kind":"mass"}],
  "targetDefaults": {"e2PgMl":{"low":100,"high":200},"tNgDl":{"low":400,"high":700},"note":"…"},
  "disclaimer": "…"
}
```

`halfLifeHours` is descriptive only; the engine never reads it. `routes` on an
ester lists the routes that produce a curve — a route missing from that list is
accepted by the parser but contributes nothing, which is what `hrt_safety`
reports.

---

### `hrt_sublingual`

Hold time and mucosal fraction are two views of one interpolation, and clients
need both directions (the settings screen stores a θ, the UI shows minutes).

**Request** — `holdMin` and/or `theta`, plus optional `params`.

**Result**

```json
{"holdMin":10.0,"theta":0.11,"nearestTier":{"name":"standard","holdMin":10.0},
 "effectiveBioavailability":0.137}
```

`effectiveBioavailability` is `θ + (1 − θ) × e2OralBio`: the part absorbed
through the mucosa plus the part that is swallowed and survives first pass.

---

### `hrt_patches`

Which removal ended which application, using the same inference as the curve.

**Request** — `{"events": […]}`.

**Result**

```json
[{"index":0,"id":"p1","applyH":498000.0,"removeH":498084.0,"wearH":84.0,
  "plannedWearH":84.0,"overWearH":0.0}]
```

`removeH`, `wearH` and `overWearH` are `null` for a patch that was never removed.

---

### `hrt_safety`

What is wrong with the record, or with the plan.

**Request** — `events`, `labs`, `bodyWeightKg`, `nowH`, `e2Target`, `tTarget`,
`params`, and optionally `startH`/`endH`.

**Result** — an array of warnings, most severe first.

```json
[{"code":"routeNotModelled","severity":"warning",
  "message":"T by oral is not modelled, …","atH":498000.0,"value":null}]
```

| Code | Severity | Trigger |
|---|---|---|
| `noDoses` | critical | No usable dose at all. |
| `nothingModelled` | critical | Every dose is a form the model ignores. |
| `noLabs` | info | No calibration possible. |
| `staleLabs` | info | Newest lab older than 180 days. |
| `implausibleWeight` | warning | Outside 30–250 kg. |
| `routeNotModelled` | warning | Route/ester pair produces zero. |
| `tInjectionNeedsEster` | warning | Unesterified testosterone injected. |
| `compoundNotModelled` | info | Spironolactone, bicalutamide, MPA, GnRH analogues. |
| `patchOverwear` | warning | Worn > planned + 24 h. |
| `cpaHighDose` | warning | Cyproterone > 50 mg/day average. |
| `doseCluster` | info | Same ester/route < 2 h apart. |
| `largeDose` | info | A single dose > 100 mg. |
| `e2PeakHigh` / `tPeakHigh` | warning | Peak > 1.25 × upper bound. |
| `e2TroughLow` / `tTroughLow` | warning | Trough < 0.7 × lower bound. |
| `e2TimeInRangeLow` / `tTimeInRangeLow` | info | Under 50% of the window in range. |

The range warnings require the matching `e2Target` / `tTarget`.

---

### `hrt_sensitivity`

How much the curve moves when one estimate is wrong.

**Request** — `events`, `parameter` (`clearance`, `weight` or `dose`), `pct`
(default 20, max 90), `bodyWeightKg`, `nowH`, optional `startH`/`endH` and
targets.

**Result** — two points, one for `+pct` and one for `−pct`.

```json
[{"parameter":"clearance","deltaPct":20.0,"peakChangePct":-9.4,
  "troughChangePct":-14.1,"aucChangePct":-11.8}, …]
```

`troughChangePct` is the useful number: if a 20% error in clearance moves the
trough by 2%, the plan is robust; if it moves it by 40%, the plan is a guess
wearing a number. Give a window (`startH`/`endH`) that excludes the empty run-in
before the first dose, or the trough will be zero and every change will read 0%.

---

### `hrt_blockade`

Anti-androgen support: is the dose in a range that is actually used, is the
record continuous, and when is the next one due.

**Request** — `events`, `bodyWeightKg`, `nowH`, optional `windowDays` (default
28), `birthYear`, `params`, `clearanceMult`. Targets are not needed: bands are
absolute daily milligrams, not concentrations.

**Result**

```json
{"compounds": [{
    "ester": "SPIRO", "name": "Spironolactone", "modelled": true,
    "doseMG": 100, "intervalH": 12, "dailyMG": 200,
    "band": "typical", "bandLowMG": 50, "bandHighMG": 200, "bandCeilingMG": 400,
    "level": {"peak": 0.41, "trough": 0.22, "mean": 0.31, "…": "…"},
    "coveragePct": 96.4, "expected": 56, "taken": 54,
    "nextDueH": 480012.0, "overdueH": 0.0,
    "verdict": "adequate",
    "mechanism": "receptorBlockade",
    "expectsTestosteroneFall": false,
    "mechanismNote": "Blocks the androgen receptor. A review of 22 studies …",
    "notes": ["…"]
  }],
 "windowDays": 28, "androgenSuppressionModelled": false, "note": "…"}
```

`band` is `low`, `typical`, `high` or `aboveCeiling`, from the daily milligram
total against per-compound bands (spironolactone 50/200/400, cyproterone
12.5/50/100, bicalutamide 25/50/150, medroxyprogesterone 2.5/10/20). `verdict`
adds the reasons a band alone cannot express: `coverage` when under 80% of the
window was logged, `nothingLogged` when there is no history at all.

`mechanism` and `expectsTestosteroneFall` are the two fields a caller should
read before drawing any conclusion. Spironolactone and bicalutamide block the
receptor and are not expected to lower testosterone; cyproterone and
medroxyprogesterone suppress gonadotropins and are. A record that credits a
receptor blocker with a fall in testosterone will read a flat curve as a dosing
failure when the dose was never the mechanism. `androgenSuppressionModelled` is
always `false`: the model has no receptor occupancy, and saying so once is
better than implying a percentage.

`nextDueH` is the last logged dose plus the median interval, not a curve
threshold — a receptor blocker has no level to cross.

---

### `hrt_progress`

Testosterone suppression and treatment effectiveness over a day, a month and a
year, each in the conservative direction.

**Request** — `events`, `labs`, `bodyWeightKg`, `nowH`, optional `e2Target`,
`tTarget`, `birthYear`, `params`, `clearanceMult`.

**Result**

```json
{"windows": [{
    "label": "day", "hours": 24, "startH": 479000, "endH": 479024,
    "labsN": 0, "tLabsN": 0, "e2LabsN": 0,
    "tMeanNgDl": null, "e2MeanPgMl": null,
    "tBelowCeiling": 0, "tCeilingNgDl": 50,
    "tSuppressionPct": null,
    "timeInRangePct": 91.2, "coveragePct": 100, "effectivenessPct": 91.2
  }, …],
 "tCeilingNgDl": 50, "note": "…"}
```

**Suppression** is the share of measured testosterone values below 50 ng/dL,
reported as the one-sided 95% Wilson lower bound rather than the raw share. One
lab under the ceiling is not 100% suppression, and the bound says so. It is
`null`, not zero, when the window holds no testosterone labs: absence of
evidence is not evidence of suppression.

**Effectiveness** is `min(timeInRangePct, coveragePct)` — the weaker of "the
model stays in range" and "the doses were actually taken". A regimen that is
perfect on paper and taken half the time is half effective; one taken perfectly
and sitting above the range is the same.

The three windows are fixed: 24 h, 720 h (30 days) and 8 760 h (365 days),
ending at `nowH`.

---

### `hrt_compare`

Several regimens, each settled before it is read.

**Request** — `arms` (1–6, each `{label?, schedule}`), `bodyWeightKg`, optional
`birthYear`, `params`, `e2Target`, `tTarget`, `maxCycles` (default 128).

```json
{"arms": [
  {"label": "weekly EV", "schedule": {"route":"injection","ester":"EV","doseMG":4,"intervalH":168}},
  {"label": "daily oral", "schedule": {"route":"oral","ester":"E2","doseMG":0.5,"intervalH":24}}
], "bodyWeightKg": 70, "e2Target": {"low": 100, "high": 200}}
```

**Result** — `arms[]` with `label`, `analyte`, `doseMG`, `intervalH`, `dailyMG`,
`weeklyMG`, `activeDailyMG`, `stable`, `cycles`, `accumulationRatio`,
`curve` (peak, trough, mean, fluctuation, time in range) and `verdict`; plus
`steadiest`, `highestMean` and `lowestWeeklyMg`.

`verdict` is `inRange`, `belowRange`, `aboveRange`, `volatile`, `notStable`,
`notModelled` or `noCurve`. `notStable` is checked first: a schedule that has
not plateaued explains its own swing.

`activeDailyMG` is the only cross-ester comparison that means anything — 2 mg of
valerate is 1.53 mg of estradiol, and `weeklyMG` ignores that.

The three summary fields deliberately answer three different questions. The
steadiest arm is rarely the cheapest and is often not the one that runs highest.

---

### `hrt_what_if`

The record with one dose changed, against the record as logged.

**Request** — `events`, `nowH`, optional `analyte` (`e2`, `t`, `cpa`, `spiro`;
default `e2`), `horizonH` (default 672), `scenarios`, `bodyWeightKg`,
`birthYear`, `params`, `clearanceMult`, targets.

```json
{"scenarios": [
  {"kind": "skipNextDose"},
  {"kind": "delayNextDose", "hours": 12},
  {"kind": "stopFor", "hours": 336}
]}
```

Kinds: `baseline`, `skipNextDose`, `doubleNextDose`, `halveNextDose`,
`delayNextDose{hours}`, `stopFor{hours}`. Omitted `scenarios` means
`baseline`, `skipNextDose`, `doubleNextDose` and a 12 h delay.

**Result** — `baseline`, `analyte`, `startH`, `horizonH`, `target`, and
`scenarios[]` with `id`, `label`, `applied`, `changes`, `curve`,
`meanDeltaPct`, `troughDeltaPct`, `peakDeltaPct`, `verdict`, `note`.

**Nothing at or before `nowH` is ever altered.** A what-if tool that rewrites
the past can make any scenario look good, so the record up to now is read-only
input. `changes` counts the doses the scenario touched; when the record has no
future dose, the row says `applied: false`, `verdict: "noChange"`.

`verdict` is `reference` (the baseline row), `lower`, `higher`, `same` (inside
±5%, which is the model's own error), `dipsBelowTarget` or `noChange`.
`dipsBelowTarget` is checked before the magnitude: taking the trough below the
range is the case worth changing the schedule for rather than the dose.

---

### `hrt_lab_review`

A panel read back rather than fitted.

**Request** — `labs`, `nowH`, optional `e2Target`, `tTarget`, `cadenceDays`
(default 90).

**Result**

```json
{"readings": [{"id": "a", "timeH": 4560, "daysAgo": 10.0,
   "analyte": "e2", "value": 320, "reported": 320, "unit": "pg/ml",
   "status": "above", "target": {"low": 100, "high": 200}}, …],
 "e2Count": 1, "tCount": 0, "e2Latest": 320, "tLatest": null,
 "e2Trend": "insufficient", "tTrend": "insufficient",
 "below": 0, "above": 1, "inRange": 0,
 "daysSinceLast": 10.0, "cadenceDays": 90,
 "cadence": "fresh", "overdueByDays": null, "note": "…"}
```

The analyte is decided by the unit: `ng/dl` and `nmol/l` are testosterone,
`pg/ml` and `pmol/l` are estradiol. Molar values are converted to the model's
unit before they are compared, so a 400 pmol/L result is judged at 109 pg/mL
rather than at 400.

`status` is `below`, `inRange`, `above` or `noTarget`. With no range supplied
the reading is reported, not judged — inventing a default range would be a
clinical claim the caller did not make.

`cadence` compares `daysSinceLast` against `cadenceDays`: `fresh`, `dueSoon`
(past 80% of the interval), `overdue` (with `overdueByDays`) or `none`. Ninety
days is a common first-year follow-up, not a rule; override it.

A trend needs two readings of the same analyte and only counts beyond 10%, which
is roughly the between-assay spread of the immunoassays these numbers usually
come from.

---

### `hrt_body`

Bust, underbust and the cup size they imply.

**Request**

```json
{"measurements": [{"timeH": 0, "bustCm": 91, "underbustCm": 76}],
 "system": "eu", "heightCm": 170, "weightKg": 63}
```

**Result**

```json
{"system": "eu", "bustCm": 91, "underbustCm": 76,
 "differenceCm": 15, "differenceIn": 5.906,
 "sizes": [
   {"system": "eu", "band": 75, "bandUnit": "cm", "cup": "C",
    "label": "75C", "differenceCm": 15, "differenceIn": 5.906},
   {"system": "cn", "band": 75, "bandUnit": "cm", "cup": "C", "label": "75C", "…": "…"},
   {"system": "uk", "band": 34, "bandUnit": "in", "cup": "A", "label": "34A", "…": "…"},
   {"system": "us", "band": 34, "bandUnit": "in", "cup": "A", "label": "34A", "…": "…"}
 ],
 "ratio": 1.197, "bmi": 21.8, "paired": true, "cupProgress": 0.75,
 "growth": {"points": 3, "spanDays": 60, "firstDifferenceCm": 10,
            "lastDifferenceCm": 12, "changeCm": 2, "rateCmPerMonth": 1.01,
            "nextCup": "B", "monthsToNextCup": 0.5},
 "note": "…"}
```

The difference between the two circumferences is the measurement; the letter is
a convention sitting on top of it. So `sizes` always carries the requested
system first and the other three after it — the same body is a 75C in Europe, a
75C in China from different arithmetic, and a 34A under the UK +4 rule, which
eats four inches of the difference before the cup is counted.

`growth` is the least-squares slope of the **difference**, not of the bust: a
tape that grows everywhere changes no cup. `monthsToNextCup` is `null` when the
rate is flat or negative, because projecting a stop is not a projection.

No volume is estimated. Turning a circumference into millilitres needs a shape
assumption that varies more between two people than between two cup sizes. There
is no target and no "normal": cup size is reported, never judged.

---

### `hrt_batch`

Several calls in one crossing of the FFI boundary.

**Request**

```json
{"calls": [
  {"function": "simulate", "input": {"events": […], "bodyWeightKg": 70, "nowH": 100}},
  {"function": "convert",  "input": {"kind": "dose", "doseMG": 2, "fromEster": "EV"}}
]}
```

`input` may be an object or a JSON string; omitted means `{}`. At most 64 calls
are executed. Each entry in `results` is a full envelope, so one failure does
not cancel the rest.

---

### Identity functions

| Function | Result |
|---|---|
| `hrt_abi_version()` | `2` (no argument) |
| `hrt_version()` | `{"ok":true,"result":"2.2.0"}` |
| `hrt_capabilities()` | `{"ok":true,"result":{"abi":2,"version":"2.2.0","functions":[…]}}` |
| `hrt_free(char *)` | nothing; releases a returned string |

---

## Limits

| Item | Limit |
|---|---|
| Samples per simulation | 100 000 (1 000 000 when `stepH` is set, clamped inside) |
| Doses per request | No hard limit; a decade of daily doses simulates in milliseconds |
| Batch calls | 64 |
| `clearanceMult` | 0.1–10 |
| `maxCycles` | 2–512 |
| `pct` for sensitivity | 0–90 |
| Arms in `hrt_compare` | 6 |
| Scenarios in `hrt_what_if` | 8 |
| Measurements in `hrt_body`, labs in `hrt_lab_review` | 500 each |
| String size | No limit; inputs are parsed directly from the caller's buffer |
