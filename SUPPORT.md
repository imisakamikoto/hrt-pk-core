# Getting help

## First, these four pages answer most questions

| If you are… | Read |
|---|---|
| calling the library for the first time | [README → Quick start](README.md#quick-start) |
| unsure what a request field means | [docs/API.md](docs/API.md) |
| unsure why the curve looks the way it does | [docs/MODEL.md](docs/MODEL.md) — including §10, what is deliberately not modelled |
| wiring it into Android, iOS, Windows, macOS, Linux, Flutter, Python or .NET | [docs/INTEGRATION.md](docs/INTEGRATION.md) |

The library also answers questions about itself: `hrt_capabilities()` returns
every function with the fields it accepts, and `hrt_catalog()` returns the
esters, routes and units with what is modelled about each.

## Then, pick the right place

| You have… | Go to |
|---|---|
| a question — "how do I…", "is this the right way to…" | [Discussions → Q&A](https://github.com/imisakamikoto/hrt-pk-core/discussions/categories/q-a) |
| an idea you are not ready to implement | [Discussions → Ideas](https://github.com/imisakamikoto/hrt-pk-core/discussions/categories/ideas) |
| something that used to work and now does not | [Issues → Bug report](https://github.com/imisakamikoto/hrt-pk-core/issues/new?template=bug_report.yml) |
| a security problem | **not** an issue — see [SECURITY.md](SECURITY.md) |
| a question about your own dose or blood test | a clinician, not this repository |

Issues are for things that are wrong. Discussions are for things that are
unclear, which is a different problem and deserves a different thread.

## What to include in a bug report

The fastest way to a fix is a request that reproduces it:

```json
{"events":[{"atH":0,"doseMG":2,"ester":"EV","route":"im"}],"bodyWeightKg":70,"nowH":168,"includeStats":true}
```

plus `hrt_version()`, `hrt_abi_version()` and the platform you ran it on. If the
answer looks wrong rather than crashing, say what you expected and why — a
number from a lab, a paper, or the app you are comparing against.

## Response times

This is a one-person project maintained around a job. Discussions and issues
usually get an answer within a week; a security report within 7 days, per
[SECURITY.md](SECURITY.md). If something has gone quiet, a comment is a
legitimate nudge.
