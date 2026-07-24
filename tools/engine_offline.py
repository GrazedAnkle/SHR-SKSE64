"""Sample-accurate sinus DSP mirror of HeartbeatVoice::Play.

Callers supply the beat state; this module does not reproduce RhythmEngine timing or morphology.
For sinus beats it applies the engine's contractility gain to S1 and keeps S2 at unit amplitude.
The `is_pvc` option is only a voice-shaping preview: it exercises the PVC bypass/resample branch but does
not reproduce RhythmEngine's per-beat PVC amplitudes or systole. WI-010 owns that parity extension.

Processing order from HeartbeatVoice.cpp:
  load -> ApplyHighPass(source low-cut) on S1 + S2 -> NormalizeJoint(S1, S2 -> SourceRestLevel) [load time]
  per beat: CompressOnsetBuild(vigor) on S1 ; ApplyLowPass(breath muffle) on S1 + S2 ;
            CopySamples(S1, amp = fs*contractility-gain) + CopySamples(S2, amp = 1.0) with per-sample
            soft-knee LAST (source -> transmission -> transducer).
Retired lobe-tamer and resonator-tail stages remain available only as explicitly
selected legacy controls so the late-S1 blind comparison is reproducible.
Constants are parsed from src/Constants.hpp. `--set Name=Value` overrides a value
for one run without changing the header.
Measurement rationale and engine/reference comparison rules live in docs/MEASUREMENT_METHODS.md.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf

import shrlib
from shrlib import SR

# Header values are the baseline; `--set Name=Value` overrides _C for one run.
_C = shrlib.parse_constants(Path(__file__).resolve().parent.parent / "src" / "Constants.hpp")

# Synthesis alias -> (Constants.hpp name, cast).
_ALIAS_MAP = {
    "S1_ON": ("S1OnsetFrames", int), "S1_END": ("S1EndFrames", int),
    "S2_ON": ("S2OnsetFrames", int), "S2_END": ("S2EndFrames", int),
    "CROSSFADE_MS": ("CrossfadeMs", float),
    "SYS_INT": ("SystoleIntercept", float), "SYS_SLOPE": ("SystoleSlope", float),
    "SYS_MIN": ("SystoleMin", float), "SYS_MAX": ("SystoleMax", float),
    "S1_SYS_FRAC": ("S1SystoleFraction", float), "S2_WIN_FRAC": ("S2WindowFraction", float),
    "ATTACK_COMPRESS_MAX": ("AttackCompressMax", float),
    "ATTACK_BUILD_THR": ("AttackBuildThreshold", float),
    "FRANK_STARLING_MIN": ("FrankStarlingMin", float), "FRANK_STARLING_MAX": ("FrankStarlingMax", float),
    "PVC_RATIO": ("ResamplePVCRatio", float),
    "BREATH_AMP_DEPTH": ("BreathAmpDepth", float), "BREATH_DEPTH_REST": ("BreathDepthRestFraction", float),
    "BREATH_PITCH_DIP": ("BreathPitchDipDepth", float),
    "BREATH_LP_OPEN": ("BreathLowPassOpenHz", float), "BREATH_LP_MIN": ("BreathLowPassMinHz", float),
    "BREATH_LP_POLES": ("BreathLowPassPoles", int),
    "CONTRACT_GAIN_DB": ("ContractilityGainDb", float),
    "SOURCE_REST_LEVEL": ("SourceRestLevel", float), "SOFT_KNEE": ("SoftClipKnee", float),
    "SRC_HIGHPASS_HZ": ("SourceHighPassHz", float),
    "VIGOR_JITTER": ("VigorJitterScale", float),
}

# Placeholders let static analysis resolve aliases populated by bind_constants().
S1_ON = S1_END = S2_ON = S2_END = BREATH_LP_POLES = 0
CROSSFADE_MS = SYS_INT = SYS_SLOPE = SYS_MIN = SYS_MAX = S1_SYS_FRAC = S2_WIN_FRAC = 0.0
ATTACK_COMPRESS_MAX = ATTACK_BUILD_THR = FRANK_STARLING_MIN = FRANK_STARLING_MAX = 0.0
PVC_RATIO = BREATH_AMP_DEPTH = BREATH_DEPTH_REST = BREATH_PITCH_DIP = 0.0
BREATH_LP_OPEN = BREATH_LP_MIN = CONTRACT_GAIN_DB = SOURCE_REST_LEVEL = SOFT_KNEE = 0.0
SRC_HIGHPASS_HZ = VIGOR_JITTER = 0.0


def bind_constants() -> None:
    """Refresh module-level synthesis aliases from _C after overrides."""
    globals().update({local: cast(_C[hpp]) for local, (hpp, cast) in _ALIAS_MAP.items()})


def apply_overrides(settings: list[str]) -> None:
    """Apply `Name=Value` overrides to _C and refresh the synthesis aliases."""
    for item in settings or []:
        name, _, value = item.partition("=")
        name = name.strip()
        if name not in _C:
            raise SystemExit(f"--set: unknown constant {name!r} (not in Constants.hpp)")
        try:
            _C[name] = float(value)
        except ValueError:
            raise SystemExit(f"--set: {name} value {value!r} is not numeric")
        print(f"  override: {name} = {_C[name]} (header default overridden)")
    bind_constants()


bind_constants()

# Frozen values from the retired late-S1 stages. These are audit fixtures, not
# live engine parameters and are intentionally not parsed from Constants.hpp.
LEGACY_TAIL_RING_LEVEL = 0.15
LEGACY_TAIL_HZ = 46.0
LEGACY_TAIL_DECAY_MS = 45.0
LEGACY_TAIL_SPLICE_MS = 78.0
LEGACY_TAIL_RAMP_MS = 15.0
LEGACY_TAIL_EXTRA_MS = 140.0
LEGACY_LOBE_DECAY_MS = 22.0
LEGACY_LOBE_ENV_MS = 2.0


def read_src(path: str | Path) -> np.ndarray:
    x, sr = shrlib.load(path)
    assert sr == SR, sr
    return x * 32768.0  # int16-magnitude float, mono

def normalize_joint(s1: np.ndarray, s2: np.ndarray, target: float) -> tuple[np.ndarray, np.ndarray]:
    pk = max(np.abs(s1).max(), np.abs(s2).max())
    if pk == 0:
        return s1, s2
    sc = 32767.0 * target / pk
    return s1 * sc, s2 * sc

def compress_onset_build(s: np.ndarray, k: float) -> np.ndarray:
    """Compress the final ascent by k while retaining the lead-in and post-peak body.

    Mirrors HeartbeatVoice::CompressOnsetBuild. The analytic envelope avoids raw
    waveform zero crossings that would stop the backward search early. `ms=0.0`
    matches the engine's unsmoothed pocketfft AnalyticEnv.
    """
    if k <= 1.0 or len(s) < 4:
        return s
    env = shrlib.env_analytic(s, SR, ms=0.0)
    pk = int(np.argmax(env))
    peak = env[pk]
    if pk == 0 or peak <= 0:
        return s
    thr = ATTACK_BUILD_THR * peak
    onset = 0
    for f in range(pk - 1, -1, -1):
        if env[f] < thr:
            onset = f + 1; break
    build_len = pk - onset
    new_len = max(1, int(build_len / k))
    if new_len >= build_len:
        return s
    idx = onset + np.arange(new_len) * build_len / new_len
    i0 = np.minimum(idx.astype(int), pk); i1 = np.minimum(i0 + 1, pk); frac = idx - i0
    build = s[i0] * (1 - frac) + s[i1] * frac
    return np.concatenate([s[:onset], build, s[pk:]])

def _box_smooth(x: np.ndarray, half: int) -> np.ndarray:
    """Edge-clamped symmetric moving average matching HeartbeatVoice::BoxSmooth."""
    if half <= 0 or len(x) == 0:
        return x
    xp = np.pad(x, (half, half), mode="edge")
    c = np.insert(np.cumsum(xp), 0, 0.0)
    w = 2 * half + 1
    return (c[w:] - c[:-w]) / w

def tame_lobe(s1: np.ndarray, strength: float) -> np.ndarray:
    """Reproduce the retired post-click S1 envelope cap for late-S1 audits.

    Only samples above the ceiling are reduced, preventing the source body from
    re-swelling into a second hump. This no longer mirrors shipping C++.
    """
    if strength <= 0.0 or len(s1) < 4:
        return s1
    mag = np.abs(s1)
    env_half = max(1, int(LEGACY_LOBE_ENV_MS * 1e-3 * SR * 0.5))
    env = _box_smooth(mag, env_half)
    pk = int(np.argmax(mag))
    r = np.exp(-1.0 / (LEGACY_LOBE_DECAY_MS * 1e-3 * SR))
    gain = np.ones(len(s1))
    ceil = env[pk]
    for f in range(pk + 1, len(s1)):
        ceil *= r
        e = env[f] if env[f] > 1e-9 else 1.0
        gain[f] = np.clip(1.0 - strength * (1.0 - min(1.0, ceil / e)), 0.0, 1.0)
    return s1 * _box_smooth(gain, max(1, env_half // 2))

def _resonator(x: np.ndarray, f0: float, tau_ms: float) -> np.ndarray:
    """Two-pole modal resonator at f0 with tau_ms decay."""
    w0 = 2.0 * np.pi * f0 / SR
    r = np.exp(-1.0 / (tau_ms * 1e-3 * SR))
    a1, a2, b0 = 2.0 * r * np.cos(w0), -(r * r), 1.0 - r
    y = np.zeros(len(x)); y1 = y2 = 0.0
    for n in range(len(x)):
        yn = b0 * x[n] + a1 * y1 + a2 * y2
        y[n] = yn; y2 = y1; y1 = yn
    return y

LEGACY_TAIL_MODES = ("fixed", "off", "dry-end")


def tail_splice_frame(dry_frames: int, mode: str = "fixed") -> int:
    """Return the legacy-tail audition splice frame.

    ``fixed`` reproduces the retired C++ behavior. ``dry-end`` preserves the
    rest-state distance between the splice and
    the end of the uncompressed source slice, so onset compression moves the
    handoff earlier with the shortened dry buffer.
    """
    if mode not in LEGACY_TAIL_MODES:
        raise ValueError(f"unknown tail mode {mode!r}; expected one of {LEGACY_TAIL_MODES}")
    fixed = int(LEGACY_TAIL_SPLICE_MS * 1e-3 * SR)
    if mode != "dry-end":
        return fixed
    baseline_dry_frames = S1_END - S1_ON
    lead_frames = max(0, baseline_dry_frames - fixed)
    return max(0, dry_frames - lead_frames)


def apply_tail(s1: np.ndarray, mode: str = "off") -> np.ndarray:
    """Reproduce the retired S1-excited resonator ring for late-S1 audits.

    ``off`` is the shipping path. ``fixed`` reproduces the retired C++ behavior;
    ``dry-end`` is the rejected splice-relative audition candidate.
    """
    if mode not in LEGACY_TAIL_MODES:
        raise ValueError(f"unknown tail mode {mode!r}; expected one of {LEGACY_TAIL_MODES}")
    if mode == "off":
        return s1
    pad = int(LEGACY_TAIL_EXTRA_MS * 1e-3 * SR)
    x = np.concatenate([s1, np.zeros(pad)])
    ring = _resonator(x, LEGACY_TAIL_HZ, LEGACY_TAIL_DECAY_MS)
    win = np.ones(len(x))
    s = tail_splice_frame(len(s1), mode)
    ramp = max(1, int(LEGACY_TAIL_RAMP_MS * 1e-3 * SR))
    win[:s] = 0.0
    win[s:s + ramp] = 0.5 - 0.5 * np.cos(np.pi * np.arange(ramp) / ramp)
    ring *= win
    rp = np.abs(ring).max()
    if rp > 0:
        ring *= LEGACY_TAIL_RING_LEVEL * np.abs(s1).max() / rp
    return x + ring

def copy_samples(dst: np.ndarray, off: int, src: np.ndarray, out_frames: int, ratio: float,
                 amp: float, cf: int, fade_in: bool, fade_out: bool) -> None:
    if out_frames <= 0:
        return
    cf = min(cf, out_frames // 2)
    f = np.arange(out_frames)
    taper = np.ones(out_frames)
    if fade_in and cf > 0:
        taper[:cf] *= (np.arange(cf) + 1) / cf
    if fade_out and cf > 0:
        taper[-cf:] *= (np.arange(cf, 0, -1)) / cf
    pos = f * ratio
    i0 = np.minimum(pos.astype(int), len(src) - 1)
    i1 = np.minimum(i0 + 1, len(src) - 1)
    frac = pos - i0
    interp = src[i0] * (1 - frac) + src[i1] * frac
    v = interp * (amp * taper) / 32768.0
    mag = np.abs(v)
    over = (mag - SOFT_KNEE) / (1.0 - SOFT_KNEE)
    knee = np.sign(v) * (SOFT_KNEE + (1.0 - SOFT_KNEE) * np.tanh(over))
    v = np.where(mag > SOFT_KNEE, knee, v)
    dst[off:off + out_frames] += v

def lowpass(buf: np.ndarray, fc: float, poles: int) -> np.ndarray:
    if fc <= 0 or poles <= 0:
        return buf
    a = 1.0 - np.exp(-2.0 * np.pi * fc / SR)
    y = buf
    for _ in range(poles):
        out = np.empty_like(y)
        acc = 0.0
        for i in range(len(y)):
            acc += a * (y[i] - acc)
            out[i] = acc
        y = out
    return y

def synth_beat(s1n: np.ndarray, s2n: np.ndarray, hr: float, contractility: float, fs: float,
               resp_phase: float = 0.0, exertion: float = 1.0, is_pvc: bool = False,
               breath_depth: float | None = None, tail_mode: str = "off",
               tamer_enabled: bool = False) -> np.ndarray:
    # Allow c > 1 so vigor jitter can create above-average-force beats. The C++
    # voice must accept the same range.
    c = np.clip(contractility, 0.0, 3.0)
    ibi = int(60.0 / hr * SR)
    systole = np.clip(SYS_INT - hr * SYS_SLOPE, SYS_MIN, SYS_MAX)
    sysf = int(systole * SR)
    cf = int(CROSSFADE_MS * 1e-3 * SR)

    # Breath transmission.
    infl = np.sin(np.pi * resp_phase)
    # Simulation callers pass `breath_depth`; `exertion` is the compatibility fallback.
    depth_state = exertion if breath_depth is None else breath_depth
    depth = BREATH_DEPTH_REST + (1 - BREATH_DEPTH_REST) * np.clip(depth_state, 0.0, 1.0)
    muffle = np.clip(depth * infl, 0, 1)
    amp_factor = 1.0 - BREATH_AMP_DEPTH * depth * infl
    pitch_dip = 1.0 - BREATH_PITCH_DIP * muffle
    # PVC dulling affects S1; breath transmission affects both sounds.
    s1_ratio = (PVC_RATIO if is_pvc else 1.0) * pitch_dip
    s2_ratio = pitch_dip
    cutoff = BREATH_LP_OPEN + (BREATH_LP_MIN - BREATH_LP_OPEN) * muffle

    # Source stage.
    s1w = s1n.copy(); s2w = s2n.copy()  # both become mutable for per-beat transmission filtering
    if not is_pvc:
        # S2 broadband brightness is drive-static in the current model.
        fs_norm = np.clip((fs - FRANK_STARLING_MIN) / (FRANK_STARLING_MAX - FRANK_STARLING_MIN), 0, 1)
        k = 1.0 + (ATTACK_COMPRESS_MAX - 1.0) * c * fs_norm  # per-beat vigor = contractility * preload
        s1w = compress_onset_build(s1w, k)
        if tamer_enabled:
            s1w = tame_lobe(s1w, min(c, 1.0))
        s1w = apply_tail(s1w, tail_mode)

    # Transmission precedes the soft knee, preserving source -> transmission ->
    # transducer order. Applies to PVCs too.
    s1w = lowpass(s1w, cutoff, BREATH_LP_POLES)
    s2w = lowpass(s2w, cutoff, BREATH_LP_POLES)

    loud = 10.0 ** ((CONTRACT_GAIN_DB / 20.0) * c)
    s1_amp = fs * loud
    s2_amp = 1.0

    s1_res = int(len(s1w) / s1_ratio)
    s1_cap = int(S1_SYS_FRAC * sysf)
    s1_frames = min(s1_res, s1_cap)
    s2_start = sysf
    s2_win = max(0, ibi - s2_start)
    s2_res = int(len(s2w) / s2_ratio)
    s2_copy = min(s2_res, int(S2_WIN_FRAC * s2_win), s2_win)

    buf = np.zeros(ibi)
    copy_samples(buf, 0, s1w, s1_frames, s1_ratio, s1_amp * amp_factor, cf, False, True)
    if s2_copy > 0:
        copy_samples(buf, s2_start, s2w, s2_copy, s2_ratio, s2_amp * amp_factor, cf, True, True)
    return buf

def _highpass_biquad(x: np.ndarray, fc: float, q: float = 0.707) -> np.ndarray:
    """Two-pole RBJ high-pass matching HeartbeatVoice::ApplyHighPass.

    Uses the same coefficients and float filter state. `fc <= 0` is passthrough.
    """
    if fc <= 0.0:
        return x
    w0 = 2.0 * np.pi * fc / SR
    cw, alpha = np.cos(w0), np.sin(w0) / (2.0 * q)
    b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
    a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for n in range(len(x)):
        yn = b0 * x[n] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, x[n]
        y2, y1 = y1, yn
        y[n] = yn
    return y


def prep_source(path: str | Path, src_highpass: float | None = None) -> tuple[np.ndarray, np.ndarray]:
    """Run the load-time chain and return normalized S1/S2 sub-samples.

    Exposed so rhythm_offline.py reuses slice -> high-pass -> joint normalize.
    """
    src = read_src(path)
    s1 = src[S1_ON:S1_END].copy()
    s2 = src[S2_ON:S2_END].copy()
    # Filter before joint normalization so gain staging accounts for the removed
    # sub-fundamental energy. An explicit corner overrides SourceHighPassHz; 0 disables it.
    fc = SRC_HIGHPASS_HZ if src_highpass is None else src_highpass
    if fc > 0.0:
        s1 = _highpass_biquad(s1, fc)
        s2 = _highpass_biquad(s2, fc)
    return normalize_joint(s1, s2, SOURCE_REST_LEVEL)


def render(path: str | Path, hr: float, contractility: float, n_beats: int = 8,
           fs: float = 1.0, exertion: float = 1.0, src_highpass: float | None = None,
           breath_depth: float | None = None, tail_mode: str = "off",
           tamer_enabled: bool = False) -> np.ndarray:
    s1, s2 = prep_source(path, src_highpass)
    out = np.concatenate([
        synth_beat(
            s1, s2, hr, contractility, fs, 0.0, exertion,
            breath_depth=breath_depth, tail_mode=tail_mode, tamer_enabled=tamer_enabled
        )
        for _ in range(n_beats)
    ])
    return out

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--hr", type=float, default=177)
    ap.add_argument("--contractility", type=float, default=1.0)
    ap.add_argument("--fs", type=float, default=1.0)
    ap.add_argument("--exertion", type=float, default=1.0)
    ap.add_argument("--breath-depth", type=float, default=None,
                    help="lagged normalized tidal-volume state [0,1]; defaults to exertion for compatibility")
    ap.add_argument("--beats", type=int, default=8)
    ap.add_argument("--set", action="append", metavar="NAME=VALUE", default=[],
                    help="override a Constants.hpp value for this run (e.g. --set SourceHighPassHz=35); "
                         "repeatable. The header stays the default.")
    ap.add_argument("--src-highpass", type=float, default=None, metavar="HZ",
                    help="override the source low-cut corner for this run (auditions a different corner; "
                         "0 = off). Default: the SourceHighPassHz constant from Constants.hpp.")
    ap.add_argument("--legacy-tail-mode", choices=LEGACY_TAIL_MODES, default="off",
                    help="late-S1 audit control; fixed/dry-end re-enable retired tail variants")
    ap.add_argument("--legacy-tamer", action="store_true",
                    help="late-S1 audit control: re-enable the retired secondary-lobe tamer")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    apply_overrides(a.set)
    y = render(
        a.source, a.hr, a.contractility, a.beats, a.fs, a.exertion,
        a.src_highpass, a.breath_depth, a.legacy_tail_mode, a.legacy_tamer
    )
    sf.write(a.out, np.clip(y, -1, 1), SR, subtype="PCM_16")
    hp_eff = SRC_HIGHPASS_HZ if a.src_highpass is None else a.src_highpass
    print(f"wrote {a.out}  HR{a.hr:.0f} c{a.contractility:.2f} fs{a.fs:.2f} hp{hp_eff:.0f} "
          f"tail={a.legacy_tail_mode} tamer={'on' if a.legacy_tamer else 'off'}  "
          f"{len(y)/SR:.2f}s  peak {np.abs(y).max():.3f}")
