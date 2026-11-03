# Security policy

## What this library can and cannot be attacked through

`hrt_core` never opens a socket, reads a file, reads the clock or the
environment. There is no service to reach and no secret to steal. That removes
most of the usual attack surface, and leaves three real ones:

1. **Hostile JSON on the boundary.** Every entry point parses caller input.
   Malformed, enormous or deliberately absurd requests must come back as an
   error envelope, never as a panic, an abort or an out-of-bounds write.
2. **The FFI contract.** A returned `char *` belongs to the library and must be
   released with `hrt_free`. A double free, or a `free()` from the host runtime,
   is a host bug that we cannot defend against — but anything the library itself
   does that makes this harder to get right is our bug.
3. **Arithmetic.** Concentrations, doses and intervals are floats from the
   caller. `NaN`, `inf`, negative doses and zero intervals must not produce
   silent nonsense in a result the caller might act on medically.

Bugs in any of those are security issues for this project, and we would like to
hear about them privately.

## Supported versions

| Version | Supported |
|---|---|
| 2.x (ABI 2) | Yes |
| 1.x (ABI 1) | No — the two entry points it had are still present, but the branch is not maintained |

Fix releases go to the latest 2.x. Prebuilt binaries for every platform are
attached to each tagged release.

## Reporting a vulnerability

**Please do not open a public issue for a security problem.**

Use GitHub's private reporting — *Security* tab → *Report a vulnerability* — on
<https://github.com/imisakamikoto/hrt-pk-core/security/advisories/new>. If that
is unavailable to you, write to **misaka-mikoto123@outlook.com** with:

* what you did, and the smallest request JSON or call sequence that shows it;
* the platform, the library version (`hrt_version()`) and the ABI
  (`hrt_abi_version()`);
* what you expected instead.

A few lines of Rust or a C snippet is ideal. You do not need a fix, and you do
not need to be sure it is exploitable — a panic on odd input is worth reporting.

## What happens next

* **Within 7 days**: an acknowledgement that the report arrived and is being
  looked at.
* **Within 30 days**: an assessment — confirmed, not reproducible, or by design —
  and, if confirmed, a fix in `main` and a patch release.
* **On release**: a GitHub Security Advisory crediting you by the name you
  choose, unless you ask to stay anonymous.

Please give us those 30 days before publishing. If a fix is going to take longer
we will say so rather than go quiet.

## Out of scope

* The correctness of the pharmacokinetic model as a description of any one
  person's body. It is a model, it is documented in
  [docs/MODEL.md](docs/MODEL.md), and it is not a medical device.
* Anything in an application that links this library — its storage, its
  permissions, its backups. Report those to that project.
* Builds you compiled yourself with modified sources.
