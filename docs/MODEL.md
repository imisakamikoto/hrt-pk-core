# The model

This document describes what the engine computes, why it is arranged this way,
and where it stops being trustworthy. It is written for someone who has to
decide whether the output is fit for their purpose — not as a clinical
reference.

## 1. Structure

Every dose is a chain of first-order steps:

```
depot ──k1──▶ ester in blood ──k2──▶ active hormone ──k3──▶ eliminated
```

- `k1` — absorption from the depot (muscle, skin, gut, mucosa).
- `k2` — hydrolysis of the ester. Zero for unesterified estradiol, and for
  routes where hydrolysis is not rate-limiting.
- `k3` — elimination from the central compartment.

Some esters show a biphasic release: a fast fraction and a slow fraction with
different `k1`. The code models that as two parallel chains sharing `k2` and
`k3`, weighted by `frac_fast`.

The amount in the active compartment after a single dose is therefore the
convolution of two or three exponentials. Rather than integrate numerically, the
engine evaluates the closed form.

## 2. Closed form

For absorption `k1` into elimination `k3` (two compartments):

```
A(τ) = −D · F · k1 · Δ₁(k1, k3, τ)
```

For the three-step chain:

```
A(τ) = D · F · k1 · k2 · Δ₂(k1, k2, k3, τ)
```

where `Δ₁` and `Δ₂` are divided differences of `s ↦ e^(−sτ)`:

```
Δ₁(x, y) = (e^(−xτ) − e^(−yτ)) / (x − y)
Δ₂(a, b, c) = (Δ₁(b, c) − Δ₁(a, b)) / (c − a)
```

The naive sum-of-exponentials form has `x − y` in a denominator, so it is `0/0`
whenever two rate constants coincide — which happens: a patch with `k1 = k3`, or
a user who sets two parameters to the same value. `Δ₂` is evaluated three ways:

1. **Far apart** `(c − a)τ ≥ 0.5` — the direct difference is well conditioned.
2. **Moderately close** — `Δ₁` uses the `sinh(z)/z` form, which is finite at
   `z = 0` and accurate near it.
3. **Nearly coincident** — `Δ₂` is evaluated from its Taylor series around the
   mean rate, to 20 terms. The series is in the small quantities `(mean − k)τ`,
   so it converges quickly exactly where the difference form would cancel.

This is why the engine has no step size and no drift: a ten-year history is
evaluated with the same arithmetic as a single dose.

## 3. Concentration

```
C = amount / Vd
```

| Analyte | `Vd` | Output unit |
|---|---|---|
| Estradiol | 2 L/kg × weight | pg/mL |
| Testosterone | 1 L/kg × weight | ng/dL |
| Cyproterone acetate | 14 L/kg × weight | ng/mL |

The conversion factors in the code (`1e9`, `1e8`, `1e6`) are just mg → pg, mg →
ng and mL → dL; they are not fitted parameters.

## 4. Rate constants

### Estradiol esters (injection)

| Ester | fast fraction | `k1` fast (h⁻¹) | `k1` slow (h⁻¹) | `k2` (h⁻¹) |
|---|---|---|---|---|
| EB (benzoate) | 0.90 | 0.144 | 0.114 | 0.09 |
| EV (valerate) | 0.40 | 0.0216 | 0.0138 | 0.07 |
| EC (cypionate) | 0.229 | 0.005035 | 0.004511 | 0.045 |
| EN (enanthate) | 0.05 | 0.001 | 0.005 | 0.015 |
| EU (undecylate) | 0.08 | 0.006 | 0.0022 | 0.012 |

Unesterified E2 has `k1 = 0.5`, `frac_fast = 1`, `k2 = 0`.

### Testosterone esters (injection)

| Ester | fast fraction | `k1` fast | `k1` slow | `k2` |
|---|---|---|---|---|
| TC (cypionate) | 0.35 | 0.025 | 0.005 | 0.2 |
| TE (enanthate) | 0.40 | 0.035 | 0.008 | 0.2 |
| TU (undecanoate) | 0.00 | ln2/(50·24) | ln2/(50·24) | 0.036 |

### Other routes

| Route | `k1` | Notes |
|---|---|---|
| Oral E2 | 0.32 h⁻¹ | EV uses 0.05 h⁻¹ and no ester pool; other esters add a pool at `k2 = ln2/14.36`. |
| Sublingual | 1.8 h⁻¹ | Fast fraction `θ`; the swallowed fraction uses the oral path. |
| Gel E2 | 0.0193 h⁻¹ | Single first-order step. |
| Gel T | 0.05 h⁻¹ | |
| Patch, first-order | 0.0075 h⁻¹ (E2), 0.03 h⁻¹ (T) | Ends at removal, then decays at `k3`. |
| Patch, zero-order | — | `rate = µg/day ÷ 24 ÷ 1000 × F`, input stops at removal. |

## 5. Bioavailability

```
F_injection(E2)  = e2Ff<ester> × MW(E2)/MW(ester)
F_injection(T)   = tFf<ester>  × MW(T)/MW(ester)
F_oral(E2)       = e2OralBio × MW(E2)/MW(ester)
F_sublingual     = [θ + (1 − θ) · e2OralBio] × MW(E2)/MW(ester)
F_gel            = siteFactor × MW(E2)/MW(ester)
F_patch          = MW(E2)/MW(ester)
```

The molecular-weight ratio is the "2 mg of valerate is 1.53 mg of estradiol"
correction: it is why a 4 mg valerate injection does not contain 4 mg of
estradiol.

Sublingual `θ` comes from the hold time, interpolated linearly between four
tiers:

| Tier | Hold | `θ` |
|---|---|---|
| quick | 2 min | 0.01 |
| casual | 5 min | 0.04 |
| standard | 10 min | 0.11 |
| strict | 15 min | 0.18 |

Beyond 15 minutes `θ` is extrapolated with a dissolution-then-swallow model,
capped at 1 and at 60 minutes of holding. The inverse (θ → hold time) is solved
by bisection, so a θ typed into a settings screen redisplays as minutes.

Gel bioavailability depends on the site: 0.05 arm, 0.05 thigh, 0.25 scrotal for
estradiol; 0.1 / 0.1 / 0.5 for testosterone.

Cyproterone acetate is oral only: `F = 0.88`, `ka = 0.5 h⁻¹`, and two
elimination phases — a fast one at 0.25 h⁻¹ weighted 5.2786 and a slow one at
`ln2/43.9` weighted 76.2, which together reproduce the published exposure.

## 6. Age

Clearance falls with age:

```
age(timeH) = 1970 + timeH / (365.2425 × 24) − (birthYear + 0.5)
multiplier = exp(−slope × (age − 54))
```

with `age` clamped to 19–75 and

- estradiol: `slope = ln(1/0.75) / (79 − 27)`
- testosterone: `slope = ln(1821/1390) / (67 − 27)`

A 25-year-old therefore clears faster than the reference 54-year-old, an
80-year-old slower. `birthYear` is optional; without it the multiplier is 1.

## 7. Patches

A patch application and its removal are separate events, and nothing on the
removal says which application it ends. The pairing is inferred:

1. Applications are ordered by time, then planned wear, then dose.
2. Each removal, in time order, closes the oldest open patch of the same
   hormone that its own planned wear still allows (`planned + 24 h` grace).
3. If none fits, it closes the newest unpaired patch of that hormone, provided
   the removal is not more than `planned + 240 h` after the application.

A patch shorter than one hour cannot be closed by a removal. An unpaired
application is assumed to stay on forever — which is deliberate: the curve then
shows the consequence of forgetting to log the removal, and `hrt_safety` reports
wear beyond the plan.

## 8. Calibration

Three methods, all returning a multiplicative correction and (for `mipd`) a
personal clearance multiplier.

**`mipd`** — grid search over `clearanceMult = exp(k)`, `k ∈ [−0.5, 0.5]` in 21
steps. For each candidate the curve is re-simulated, the log residuals
`ln(obs/model)` are computed, and the amplitude is their mean — the
least-squares optimum for a pure scale. The candidate with the smallest residual
spread wins. Requires at least two labs spanning 48 hours; below that the fit
falls back to the geometric mean of the ratios, because one number cannot
identify both amplitude and clearance.

**`ekf`** — scalar Kalman filter on the log amplitude, one update per lab:
process noise `q = 1e-6` per hour, measurement variance `r = 0.04` (≈20%). The
state after the last lab is the correction.

**`ou_kalman`** — each lab pulls on the estimate with weight
`exp(−|t − t_lab| / 336 h)`, so a result from a fortnight ago counts less than
one from yesterday. `forward` mode produces one factor per lab for a chart with
a moving correction.

Corrections are clamped to `e^±3` (≈0.05–20×): beyond that the input is wrong,
not the model.

A lab is only used where the model predicts a non-zero level. A sample taken
before the first dose, or on a route the model ignores, is dropped rather than
allowed to drag the scale towards infinity.

## 9. Statistics

- **AUC** — trapezoidal over the sampled window, estradiol only.
- **mean** — `AUC / span`, time-weighted, not the arithmetic mean of samples.
- **time in range** — for each segment, the overlap of the linear interpolation
  with `[low, high]` is computed exactly, so the answer does not change when the
  sample spacing does.
- **fluctuation** — `(peak − trough) / trough`.

The default grid adds samples at 0.25, 0.5, 1, 2, 4, 6, 8, 12, 24 and 48 hours
after every dose so a fast peak (sublingual, immediate-release oral) is not
missed by a coarse uniform grid. A caller-supplied `stepH` replaces that grid.

## 10. What is not modelled

Being explicit about this matters more than the equations above.

- **No DHT, no SHBG, no free-hormone calculation.** Only total estradiol and
  total testosterone.
- **No first-pass metabolites.** Oral estradiol is treated as a lower
  bioavailability, not as estrone and conjugates with their own kinetics.
- **No injection-site or carrier effects** beyond the ester's fast/slow split.
  Oil, suspension, viscosity and volume are not inputs.
- **No interactions.** Enzyme inducers, inhibitors, anti-androgens and
  progestins do not change the curve. Cyproterone is modelled on its own, not
  for what it does to testosterone.
- **No intra-individual variability.** A person is one set of constants, not a
  distribution.
- **No absorption lag.** A dose starts contributing at its logged moment; a
  real depot has a delay of minutes to hours.
- **Unesterified testosterone cannot be injected** in this model — there is no
  parameter set for an aqueous or crystalline depot. The dose is accepted and
  contributes nothing; `hrt_safety` says so.
- **Spironolactone, bicalutamide, MPA and GnRH analogues are not modelled.**
  Doses are stored, no curve is produced.
- **Not a dosing recommendation.** `hrt_plan_dose` solves the model; it does not
  know the person, the indication, the interactions or the guidelines.

## 11. Where the numbers come from

The rate constants, fractions and bioavailabilities are a port of the model in
[Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker),
which itself follows published pharmacokinetic work on estradiol esters,
transdermal gel, patches and cyproterone acetate. They are population values
chosen to reproduce published curves, not fitted to any individual.

The volumes of distribution (2 L/kg for estradiol, 1 L/kg for testosterone, 14
L/kg for cyproterone) and the age slopes come from the same source.

If you change a constant, change it in `src/model.rs` (defaults) or
`src/engine.rs` (rate constants), and add a test that pins the resulting curve
to a plausible range — the existing tests do this for oral estradiol, weekly
valerate and patch pairing.
