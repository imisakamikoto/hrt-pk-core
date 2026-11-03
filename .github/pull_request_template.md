## What this changes

<!-- One paragraph. What was wrong, or what is now possible. -->

## Why

<!--
If it fixes a bug: the request JSON that showed it, and what it returned.
If it adds a capability: the question a caller could not answer before.
-->

## How

<!--
Only if the diff is not obvious. Call out anything a reviewer should look at
twice: a changed constant, a new branch in the maths, anything touching the
exported symbols.
-->

## Checks

- [ ] `cargo fmt --check` passes
- [ ] `cargo test --release` passes (new behaviour has a test named after it)
- [ ] `bash scripts/verify.sh` passes on my machine
- [ ] `CHANGELOG.md` has an entry under `Unreleased`
- [ ] If an exported function changed: `include/hrt_core.h`, `docs/API.md`,
      `bindings/` and the `functions` list in `capabilities()` (`src/api.rs`) are
      updated together
- [ ] If an existing caller would have to change: `ABI_VERSION` is bumped and
      this is called out above

## Does it touch the model?

<!-- Delete whichever does not apply. -->

- No — docs, bindings, build or plumbing only.
- Yes — and here is the evidence the numbers are right: <!-- lab values, a paper, a plot -->
