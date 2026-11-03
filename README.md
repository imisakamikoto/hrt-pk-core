# hrt-pk-core

<img src="assets/logo.png" width="88" align="right" alt="hrt-pk-core">

**English** · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md)

[![build](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml/badge.svg)](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml)
[![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ABI](https://img.shields.io/badge/ABI-2-informational)](docs/API.md)
[![platforms](https://img.shields.io/badge/Android%20·%20iOS%20·%20Windows%20·%20macOS%20·%20Linux-lightgrey)](#install)

An offline pharmacokinetic engine for estradiol and testosterone HRT, packaged
as a small native library with a JSON-over-C ABI.

No network, no clock, no filesystem, no global state. You hand it a dosing
history and it hands back a curve, a summary, a plan, or a warning — as JSON.
The same code runs on Android, iOS, Windows, macOS and Linux, and the exported
symbols are plain C, so anything with an FFI can call it.

```
┌─────────────┐   JSON    ┌──────────────────────────────────────────┐
│ your app    │ ────────▶ │ hrt_core                                 │
│ Dart / Py   │           │  model → engine → calibration            │
│ C# / C / …  │ ◀──────── │  stats · planning · reminders · safety   │
└─────────────┘   JSON    └──────────────────────────────────────────┘
```

- **ABI 2**, library version `2.0.0`, 19 exported functions.
- 1.5 MB–21 MB depending on the platform; no runtime dependencies beyond libc.
- Deterministic: the same input gives the same output, on every platform.
- Panics are caught at the boundary and returned as an error object.

> This is a model, not medical advice. It estimates concentrations from what it
> is told and cannot know anything that was not recorded.

---

## What it can answer

| Question | Function |
|---|---|
| What is my level now, and what will it be next Tuesday? | `hrt_simulate` |
| What were my peak, trough, average and time-in-range? | `hrt_stats` |
| My labs disagree with the model — by how much? | `hrt_calibrate` |
| If I inject every 7 days, where does it settle? | `hrt_steady_state` |
| What dose would keep my trough at 120 pg/mL? | `hrt_plan_dose` |
| When should the app remind me to take the next dose? | `hrt_reminders` |
| Have I actually been taking it as planned? | `hrt_adherence` |
| How long will this vial last? | `hrt_supply` |
| 150 pg/mL is how many pmol/L? | `hrt_convert` |
| Which esters, routes and units exist? | `hrt_catalog` |
| How long should I hold it under my tongue? | `hrt_sublingual` |
| Which removal ended which patch? | `hrt_patches` |
| Is there something in this record the model ignores? | `hrt_safety` |
| Which estimate does the answer depend on? | `hrt_sensitivity` |
| All of the above, without 12 FFI crossings. | `hrt_batch` |

## Documentation

| | |
|---|---|
| [API reference](docs/API.md) | Every function, every field, every error code |
| [The model](docs/MODEL.md) | Equations, constants, and what is *not* modelled |
| [Integration](docs/INTEGRATION.md) | Per-platform packaging, loading, memory and threading |
| [Developing](docs/DEVELOPING.md) | Build, test, add a function, release |
| [Changelog](CHANGELOG.md) | What changed between ABI versions |

## Quick start

### C

```c
#include "hrt_core.h"

char *out = hrt_simulate(
    "{\"events\":[{\"id\":\"a\",\"route\":\"injection\",\"timeH\":0,"
    "\"doseMG\":4,\"ester\":\"EV\"}],\"bodyWeightKg\":70,\"nowH\":72}");
if (out) {
    puts(out);          /* {"ok":true,"result":{"timeH":[…],…}} */
    hrt_free(out);      /* every returned string must be freed */
}
```

### Python

```python
from hrt_core import HrtCore

core = HrtCore()                     # or HrtCore("/path/to/libhrt_core.so")
curve = core.simulate(
    events=[{"id": "a", "route": "injection", "timeH": 0, "doseMG": 4, "ester": "EV"}],
    bodyWeightKg=70,
    nowH=72,
)
print(max(curve["e2PgMl"]), "pg/mL")
```

### Flutter / Dart

```dart
final core = HrtCore.open();
final curve = core.simulate({
  'events': [
    {'id': 'a', 'route': 'injection', 'timeH': 0, 'doseMG': 4, 'ester': 'EV'},
  ],
  'bodyWeightKg': 70,
  'nowH': 72,
});
```

### C#

```csharp
var core = new HrtCore();
var curve = core.Simulate(new {
    events = new[] { new { id = "a", route = "injection", timeH = 0.0, doseMG = 4.0, ester = "EV" } },
    bodyWeightKg = 70.0,
    nowH = 72.0,
});
```

### Rust

```toml
[dependencies]
hrt-pk-core = { path = "…" }   # or git = "https://github.com/imisakamikoto/hrt-pk-core"
```

```rust
use hrt_core::engine::run_simulation;
use hrt_core::model::{DoseEvent, Ester, PkParams, Route};

let event = DoseEvent {
    id: "a".into(), route: Route::Injection, time_h: 0.0,
    dose_mg: 4.0, ester: Ester::EV, extras: Default::default(), updated_at: None,
};
let sim = run_simulation(&[event], 70.0, None, &PkParams::default(), 72.0, 1.0).unwrap();
```

More: [`docs/API.md`](docs/API.md) for every field, [`docs/INTEGRATION.md`](docs/INTEGRATION.md)
for per-platform packaging, [`docs/MODEL.md`](docs/MODEL.md) for the maths,
[`docs/DEVELOPING.md`](docs/DEVELOPING.md) for building and releasing.

## Install

### Prebuilt binaries

Every tagged release carries:

| File | Platform |
|---|---|
| `libhrt_core.so` (arm64-v8a, armeabi-v7a, x86_64) | Android 7+ |
| `libhrt_core.a` (device arm64, simulator universal) | iOS 15+ |
| `hrt_core.dll` + import library | Windows 10 1809+ x64 |
| `libhrt_core.so` | Linux x86_64 |
| `libhrt_core.dylib` (universal) | macOS 11+ |
| `hrt_core.h` | every platform |

### From source

```bash
cargo build --release              # host library in target/release/
cargo test                         # 32 unit tests + 1 doc test
bash scripts/build-all.sh          # every cross-target (needs the toolchains)
```

## Design

**JSON at the boundary, types inside.** A C ABI of structs would need a version
of every struct per platform and a hand-written marshaller in every language.
One JSON object in, one JSON object out, and the same six lines of binding code
work in any language. The cost — parsing — is microseconds against a simulation
that samples thousands of points.

**Closed form, not numerical integration.** Each dose is a chain of first-order
steps (absorption → ester hydrolysis → elimination) solved analytically, with
divided differences at coincident rate constants. Two compartments are a
Bateman expression; three are the second divided difference of `e^(−sτ)`. No
step size to tune, no drift, no accumulation error over a decade of history.

**The curve is the source of truth.** Peak, trough, AUC, time-in-range and the
reminder times are all derived from the same samples the chart is drawn from,
so a summary can never disagree with the curve next to it.

**Nothing is hidden.** A dose the model will not use is reported by
`hrt_safety` instead of silently contributing zero; a route that ignores the
logged amount says so; a schedule that cannot fit a range returns the conflict
rather than a plausible-looking dose.

## The model at a glance

- Estradiol and testosterone share one structure: `k1` absorption, optional
  `k2` ester hydrolysis, `k3` elimination, with fast and slow fractions for
  esters that show a biphasic release.
- Volume of distribution: 2 L/kg (E2), 1 L/kg (T), 14 L/kg (CPA).
- Routes: injection, oral, sublingual (with a mucosal fraction from hold time),
  gel (site-dependent bioavailability), patch (first-order or zero-order from a
  labelled release rate, paired with its removal).
- Clearance rises with age relative to 54 years, and can be multiplied by a
  personal factor from calibration.
- Calibration: `mipd` (clearance grid + closed-form amplitude), `ekf` (scalar
  Kalman on log residual), `ou_kalman` (time-decayed weighting), or `off`.

Details, constants and limits: [`docs/MODEL.md`](docs/MODEL.md).

## Compatibility

The JSON field names match the storage and backup format of
[Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker),
including `doseMG` with capital letters, so a backup can be replayed without a
translation layer. `ABI_VERSION` is bumped whenever a field changes meaning.

## Related

- **[Misaka hrt记录器](https://github.com/imisakamikoto/misaka-hrt-tracker)** — the
  Android, iOS and Windows app this library was extracted from. It embeds the
  core, keeps the data on the device, and is where the JSON shapes come from.
- **[Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker)** —
  the browser application whose model this is a port of.

## Contributing

Small library, narrow job. [CONTRIBUTING.md](CONTRIBUTING.md) says what is in
scope and lists the five rules the code lives by — no clock, no I/O, no panic
across the boundary, analytic maths, and a summary that never contradicts its
curve.

- **Questions and ideas** → [Discussions](https://github.com/imisakamikoto/hrt-pk-core/discussions); [SUPPORT.md](SUPPORT.md) explains which channel to use.
- **Bugs** → [issues](https://github.com/imisakamikoto/hrt-pk-core/issues/new?template=bug_report.yml), ideally with the request JSON that reproduces it.
- **Security** → privately, per [SECURITY.md](SECURITY.md), never in a public issue.
- **Conduct** → [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

If you use this in published work, [CITATION.cff](CITATION.cff) has an entry you
can cite.

## Licence

MIT. The pharmacokinetic model is a port of Oyama's HRT Tracker by Joseph
Smirnova Oyama; see [`NOTICE.md`](NOTICE.md) for the full attribution and the
list of changes.
