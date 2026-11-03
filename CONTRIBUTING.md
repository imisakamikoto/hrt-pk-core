# Contributing

Thanks for looking. This is a small library with a narrow job: given doses and
labs, answer questions about the curves. Anything that keeps it doing that
better — a fixed constant, a clearer error, a binding for a language we do not
cover, a sentence in the docs — is welcome.

## Before you start

* **Bug in the maths?** Open an issue with the numbers that disagree. A minimal
  request JSON and the curve it produced is worth more than a description.
* **New capability?** Open an issue first and say what question it answers.
  The bar is "a caller cannot compute this from what is already returned",
  not "it would be convenient".
* **Typo or unclear sentence?** Just send the pull request, no issue needed.

Please do not open a pull request that reformats files, bumps dependencies and
fixes a bug at the same time. One reason per pull request.

## Getting set up

```bash
git clone https://github.com/imisakamikoto/hrt-pk-core
cd hrt-pk-core
cargo test                  # 32 unit tests + 1 doc test
bash scripts/verify.sh      # everything CI runs, locally
```

Rust 1.75 or newer. Nothing else is required for the core; the C, Python and
.NET examples need a compiler or runtime, and the cross-compilation targets need
the toolchains listed in [docs/DEVELOPING.md](docs/DEVELOPING.md).

## The rules the code lives by

These are not style preferences, they are the reason the library can be trusted
by five different runtimes:

1. **No clock, no network, no filesystem, no environment.** Time is an argument
   (`nowH`). If your change needs to know what time it is, the change belongs in
   the caller.
2. **Every export takes and returns JSON, and never panics across the boundary.**
   A bad input is a `{"ok":false,...}` answer, not a crash in someone's app.
3. **The maths stays analytic.** No iterative fitting where a closed form
   exists, no hidden state between calls.
4. **A summary never contradicts its curve.** Peak, trough, AUC and time in range
   are derived from the same samples the caller receives.
5. **Say what is not modelled.** If a route or an ester cannot be represented,
   the answer says so rather than quietly treating it as zero.

## Making a change

1. Fork, then branch from `main`.
2. Write the test first if you can — one test per behaviour, named after the
   behaviour, not after the function.
3. Keep the diff small. `cargo fmt` before you commit; CI runs
   `cargo fmt --check`.
4. Add a line to [CHANGELOG.md](CHANGELOG.md) under `Unreleased`.
5. Open the pull request and fill in the template. CI must be green: format,
   tests, the C ABI example, the Python binding, and the symbol check on every
   platform.

Adding an exported function has its own checklist — eight steps, in
[docs/DEVELOPING.md](docs/DEVELOPING.md#adding-a-function). Please read it
before you write the `extern "C"`.

## Changing the ABI

`ABI_VERSION` only goes up when an existing caller would have to change. Adding
a function does not bump it; changing a field name or a return shape does. If
your change needs a bump, say so in the pull request and update
`include/hrt_core.h`, `docs/API.md` and every binding in the same commit.

## Reviews

One maintainer, so reviews are honest but not instant. Expect a reply within a
week. If a pull request goes quiet, comment on it — that is a nudge, not a
nuisance.

By contributing you agree that your work is licensed under the MIT Licence, as
described in [LICENSE](LICENSE).
