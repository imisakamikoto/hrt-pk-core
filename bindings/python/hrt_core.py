"""Python binding for hrt_core.

    from hrt_core import HrtCore
    core = HrtCore()
    curve = core.simulate(events=[...], body_weight_kg=70, now_h=12345.0)

The binding is a thin ctypes wrapper: it marshals a dict to JSON, calls the
shared library, and raises `HrtError` when the envelope says `ok: false`.
No state is kept between calls, and the library is safe to use from several
threads (each call is independent).
"""

from __future__ import annotations

import ctypes
import json
import os
import platform
import sys
from pathlib import Path
from typing import Any, Iterable


class HrtError(RuntimeError):
    """The library rejected a request; `code` is the machine-readable reason."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(f"{code}: {message}" if code else message)
        self.code = code
        self.message = message


def _library_name() -> str:
    system = platform.system()
    if system == "Windows":
        return "hrt_core.dll"
    if system == "Darwin":
        return "libhrt_core.dylib"
    return "libhrt_core.so"


def _candidate_paths(name: str) -> Iterable[Path]:
    override = os.environ.get("HRT_CORE_LIB")
    if override:
        yield Path(override)
    here = Path(__file__).resolve().parent
    for base in (here, here.parent, here.parent.parent):
        yield base / name
        yield base / "target" / "release" / name
        yield base / "dist" / name
    yield Path(name)  # let the loader search the system paths


class HrtCore:
    """A loaded copy of the engine."""

    def __init__(self, path: str | os.PathLike[str] | None = None) -> None:
        self._lib = self._load(path)
        self._bind()

    # --- loading ---------------------------------------------------------

    @staticmethod
    def _load(path: str | os.PathLike[str] | None) -> ctypes.CDLL:
        name = _library_name()
        candidates = [Path(path)] if path else list(_candidate_paths(name))
        errors: list[str] = []
        for candidate in candidates:
            try:
                if candidate.exists() or str(candidate) == name:
                    return ctypes.CDLL(str(candidate))
            except OSError as exc:  # wrong architecture, missing dependency…
                errors.append(f"{candidate}: {exc}")
        raise OSError(
            "could not load hrt_core; set HRT_CORE_LIB or build it with "
            "`cargo build --release`. Tried: " + ", ".join(errors or [str(c) for c in candidates])
        )

    def _bind(self) -> None:
        self._lib.hrt_simulate.restype = ctypes.c_void_p
        self._lib.hrt_simulate.argtypes = [ctypes.c_char_p]
        for name in _FUNCTIONS:
            fn = getattr(self._lib, name)
            fn.restype = ctypes.c_void_p
            fn.argtypes = [ctypes.c_char_p]
        self._lib.hrt_version.restype = ctypes.c_void_p
        self._lib.hrt_version.argtypes = []
        self._lib.hrt_capabilities.restype = ctypes.c_void_p
        self._lib.hrt_capabilities.argtypes = []
        self._lib.hrt_free.restype = None
        self._lib.hrt_free.argtypes = [ctypes.c_void_p]
        self._lib.hrt_abi_version.restype = ctypes.c_int32
        self._lib.hrt_abi_version.argtypes = []

    # --- plumbing --------------------------------------------------------

    def _release(self, pointer: int) -> None:
        self._lib.hrt_free(ctypes.c_void_p(pointer))

    def _decode(self, pointer: int) -> dict[str, Any]:
        if not pointer:
            raise HrtError("null", "the library returned a null pointer")
        try:
            raw = ctypes.string_at(pointer)
        finally:
            self._release(pointer)
        envelope = json.loads(raw.decode("utf-8"))
        if not envelope.get("ok"):
            raise HrtError(envelope.get("code", ""), envelope.get("error", "unknown error"))
        return envelope.get("result")

    def _call(self, function: str, request: dict[str, Any] | None) -> Any:
        payload = json.dumps(request or {}, ensure_ascii=False).encode("utf-8")
        pointer = getattr(self._lib, function)(payload)
        return self._decode(pointer)

    # --- identity --------------------------------------------------------

    @property
    def version(self) -> str:
        return self._decode(self._lib.hrt_version())

    @property
    def abi_version(self) -> int:
        return int(self._lib.hrt_abi_version())

    def capabilities(self) -> dict[str, Any]:
        return self._decode(self._lib.hrt_capabilities())

    # --- the API ---------------------------------------------------------

    def simulate(self, **request: Any) -> dict[str, Any] | None:
        """Concentration curves. See docs/API.md for every field."""
        return self._call("hrt_simulate", request)

    def stats(self, **request: Any) -> dict[str, Any] | None:
        return self._call("hrt_stats", request)

    def calibrate(self, **request: Any) -> dict[str, Any]:
        return self._call("hrt_calibrate", request)

    def steady_state(self, **request: Any) -> dict[str, Any] | None:
        return self._call("hrt_steady_state", request)

    def plan_dose(self, **request: Any) -> dict[str, Any] | None:
        return self._call("hrt_plan_dose", request)

    def reminders(self, **request: Any) -> dict[str, Any]:
        return self._call("hrt_reminders", request)

    def adherence(self, **request: Any) -> dict[str, Any] | None:
        return self._call("hrt_adherence", request)

    def supply(self, **request: Any) -> dict[str, Any] | None:
        return self._call("hrt_supply", request)

    def convert(self, **request: Any) -> dict[str, Any]:
        return self._call("hrt_convert", request)

    def catalog(self) -> dict[str, Any]:
        return self._call("hrt_catalog", {})

    def sublingual(self, **request: Any) -> dict[str, Any]:
        return self._call("hrt_sublingual", request)

    def patches(self, events: list[dict[str, Any]]) -> list[dict[str, Any]]:
        return self._call("hrt_patches", {"events": events})

    def safety(self, **request: Any) -> list[dict[str, Any]]:
        return self._call("hrt_safety", request)

    def sensitivity(self, **request: Any) -> list[dict[str, Any]]:
        return self._call("hrt_sensitivity", request)

    def blockade(self, **request: Any) -> dict[str, Any]:
        """Anti-androgen dose bands, coverage and the next dose due."""
        return self._call("hrt_blockade", request)

    def progress(self, **request: Any) -> dict[str, Any]:
        """Suppression and effectiveness over a day, a month and a year."""
        return self._call("hrt_progress", request)

    def compare(self, **request: Any) -> dict[str, Any]:
        """Up to six regimens, each read after it settles."""
        return self._call("hrt_compare", request)

    def what_if(self, **request: Any) -> dict[str, Any]:
        """The record with one dose changed, against the record as logged."""
        return self._call("hrt_what_if", request)

    def lab_review(self, **request: Any) -> dict[str, Any]:
        """A lab panel read back rather than fitted."""
        return self._call("hrt_lab_review", request)

    def body(self, **request: Any) -> dict[str, Any]:
        """Bust, underbust and the cup size they imply."""
        return self._call("hrt_body", request)

    def batch(self, calls: list[dict[str, Any]]) -> list[dict[str, Any]]:
        """Several calls at once; failures come back inline, not as exceptions."""
        result = self._call("hrt_batch", {"calls": calls})
        return result["results"]


_FUNCTIONS = (
    "hrt_simulate",
    "hrt_stats",
    "hrt_calibrate",
    "hrt_steady_state",
    "hrt_plan_dose",
    "hrt_reminders",
    "hrt_adherence",
    "hrt_supply",
    "hrt_convert",
    "hrt_catalog",
    "hrt_sublingual",
    "hrt_patches",
    "hrt_safety",
    "hrt_sensitivity",
    "hrt_blockade",
    "hrt_progress",
    "hrt_compare",
    "hrt_what_if",
    "hrt_lab_review",
    "hrt_body",
    "hrt_batch",
)


if __name__ == "__main__":
    core = HrtCore()
    print("version", core.version, "abi", core.abi_version)
    curve = core.simulate(
        events=[
            {"id": "a", "route": "injection", "timeH": 0, "doseMG": 4, "ester": "EV"},
        ],
        bodyWeightKg=70,
        nowH=72,
    )
    peak = max(curve["e2PgMl"]) if curve else None
    print("peak", round(peak, 1) if peak else None, "pg/mL")
    sys.exit(0)
