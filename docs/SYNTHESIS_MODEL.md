# Synthesis Model

How physiological state is mapped to sound, the mechanisms that create this mapping, and why.
Pairs with [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md) for measurements and
[ROADMAP.md](ROADMAP.md) for task status. For term definitions see
[GLOSSARY.md](GLOSSARY.md).

## Sourcing and Shaping

Our synthesis source is one recording at one physiological state (`HeartBeat_Shortened.wav`,
a resting low-HR "thud", spectral centroid about 51 Hz). A reference recording's spectrum mixes
two things we must treat differently:

- **Recording-chain timbre** (mic or stethoscope response, tissue transmission, codec). This is
  the capture path, not physiology. Matching it via EQ is what makes a sound read as "a real heart
  recording" and is safe to bake in as a static baseline.
- **Physiological state** (especially S1 loudness, envelope, and brightness, which rise with
  contractility). This must be modulated at runtime, never frozen. S2's broadband spectrum is static
  with drive; its one drive-responsive spectral feature is the site-dependent P2 clap (coupling 2).

We keep the source's resting timbre as the baseline and modulate brightness from there as a function
of state. Heart sounds are band-limited (essentially nothing above ~200 Hz) and brightness is set by
the source's spectrum *after the low-cut* (below).

The **primary brightness lever is a static source low-cut** (`SourceHighPassHz`,
`PrepareHeartbeatSource` / `ApplyHeartbeatSourceHighPass`) - the recording-chain-timbre correction this
section opened with. The
source's S1 fundamental sits near 22 Hz with ~37% of its energy below 40 Hz, roughly an
octave below real hearts (references 62-75 Hz) which is indicative of capture-path sub-bass coloration
(contact-transducer proximity resonance / no subsonic filter / cinematic sub weight). A 2-pole
Butterworth high-pass on the S1/S2 sub-samples at load (before joint normalization, so the
renormalization gain-stages for the removed energy) removes that boom so the source's own
fundamental reads clear at ~76 Hz. It is *not* a resample/pitch shift (cf. coupling 2). The sub-bass
octave masks the source's own upper harmonics; the cut therefore un-dulls the source and lifts its
HF fraction. The rendered S1 sits **mid-range among the references** for HF content without an
exciter. `SourceHighPassHz` preserves the attack and leaves fundamental headroom. The post-peak
source body also requires no tamer or synthetic tail; the intact body is the perceptually preferred
match to ref8 across rest, high drive, natural breathing, and recovery. S2 conditioning remains a
separate decision rather than changing the adopted S1 corner.

**Runtime drive-brightening** - the rise of S1 brightness with contractility - is produced by the
contractility-scaled **onset compression** (coupling 5), which sharpens the S1 envelope: a steeper
pressure rise and more abrupt blood/tissue deceleration can produce a sharper transient, which carries
more HF. The source's post-peak lobes and decay remain intact. This is a physiologically compatible
working mechanism, grounded directionally in the ref11 and ref15 within-recording drive pairs; those
recordings do not isolate the exact causal decomposition or coefficient. The exact ref11 magnitude
remains method-sensitive; see [WI-005](work_items/WI-005-cross-gap-audit.md).

**Resampling is reserved for *pitch* motion** - the breath pitch-dip and PVC dulling - not for
brightness: speeding the sample up shifts the fundamental, which reads as sped-up rather than forceful.

The cardiac-source chain has no **harmonic exciter**. A contractility-scaled tanh waveshaper would
manufacture 100-200 Hz harmonics even though the source low-cut already places S1 HF content in the
reference range, and it would concentrate that HF in the lobe *body* rather than the rise. That temporal
placement is the perceptual sign opposite a sharper attack. Breath muffle must be evaluated on this
exciter-free spectrum under [WI-008](work_items/WI-008-spectral-breath-muffle.md).

## Signal-chain architecture

Every acoustic effect belongs to one of three physical stages, and getting an effect into the right
stage is what keeps the model coherent. The stages run in order, and processing must follow that
order.

1. **Cardiac source (the heart itself).** Loudness, brightness, and attack sharpness. A more
   forceful contraction is louder, can produce a sharper pressure/deceleration transient, and genuinely
   emits more high-frequency energy. These are real properties of the emitted sound and all scale
   with **contractility** (and per-beat preload via Frank-Starling). Grounding: ref11's centroid
   rises within the ref11 and ref15 drive pairs (same recording/site; directional, with the ref11
   magnitude under [cross-gap audit](work_items/WI-005-cross-gap-audit.md)), so the brightening is cardiac,
   not merely a cross-recording timbre difference; ref13's HR rise shows loudness and centroid climbing
   with exertion, and the attack sharpening is re-established on the within-recording drive pairs
   (ref11 -21.9 ms, ref14 -7.4 ms, measured as the null-immune 10-90% rise). These drive **S1**;
   **S2 loudness is held fixed** (S1 amplitude scales with contractility and Frank-Starling, S2 stays
   at unit amplitude), so the S2/S1 loudness ratio falls as drive rises - as the references show.
2. **Transmission (chest, lung, tissue between heart and sensor).** The breath effect lives here:
   an inspiratory low-pass **muffle** (a large directional centroid drop at deep inspiration), a smaller
   **pitch dip** (currently modeled, but weakly supported - see coupling 1), and breath
   **amplitude attenuation**. All scale with **lung inflation / tidal volume**, represented by the
   lagged `Simulation::m_RespDepth` state, so they deepen with exertion and remain elevated into
   recovery before returning to shallow resting breaths.
3. **Transducer / recording chain (the "virtual stethoscope").** A **soft-knee limiter** at the *end*
   of the chain (`SoftClipKnee`), whose role is **clip-prevention** - keeping the loudest jittered beats
   off the digital ceiling. It is **level-driven**, so it engages *emergently* with loudness (a louder
   beat drives it harder) rather than via any contractility mapping. Physically it stands in for
   transducer/preamp saturation, a **recording artifact, not emitted by the heart** (a healthy heart
   does not clip). That grounding as capture, not physiology, is why equally forceful loud beats saturate
   or not by each recording's gain-staging: ref8's loud beats keep a clean crest (~11.0 dB; their punch
   is rise/body HF *contrast*, not a faster attack - ref8's 10-90% rise is 33.9 ms, comparable to ours), while
   ref12's are clearly saturated (~9.0 dB).

   It does **not** add audible grit at the current operating point. With vigor jitter live, only the
   jitter's upper tail reaches the knee at all; the loudest beat's nonlinear residual is ~-28 dB
   (~4% THD), and renders across `SoftClipKnee` 0.30-0.50 and `ContractilityGainDb` 11-13 are
   indistinguishable by ear. Drive is heard as *level*, not saturation, and grit is not a design
   goal for the **default** sound profile - a deliberately saturated profile remains a coherent
   future option (see [additional audio realism](ROADMAP.md#additional-audio-realism)).

   The operating point (`ContractilityGainDb` driving the fixed `SoftClipKnee`) is set so the loudest
   beats land in the reference crest range rather than flat-topping into a plateau. It must be re-checked
   whenever the S1 envelope energy changes - onset compression changes its peak-to-body balance, and a
   level tuned against a different envelope can over-saturate (crest crushed, samples pinned near full
   scale). It is co-calibrated with the beat-to-beat vigor jitter (coupling 12), whose upward
   loudness excursions need room below the ceiling: at maximum drive the mean loud beat otherwise
   sits near full scale and clips those excursions, crushing the loudness spread and skewing the
   beat-to-beat distribution. `SourceRestLevel` sets the resting level low enough to leave that
   headroom. An in-game rest-to-exercise-to-recovery capture validates the adopted operating point;
   any envelope change must repeat that gate.

   `VoiceOutputGain` is downstream engine integration rather than heart physiology: it compensates the
   game's roughly quarter-scale mix attenuation while leaving the user-facing volume default at unity.
   Because `SoftClipKnee` has already bounded the rendered buffer, the reciprocal mix attenuation is the
   downstream clip-free ceiling. A game-mix change therefore requires a post-exercise output-peak check,
   not a retune of cardiac-source loudness.

The output **soft-knee** is the current chain's only tanh stage. It is level-driven and belongs to the
transducer stage; drive-brightening belongs to onset compression in the cardiac-source stage.

**Processing order:** source (loudness, brightness via onset shaping, attack) -> transmission
(muffle, pitch dip, breath attenuation) -> transducer (limiter, last). This ordering is why a loud
expiration beat saturates (it reaches the transducer at full level) while a muffled inspiration beat
does not (transmission has already attenuated it) - for free, without special-casing.

## Factor-to-feature map

Input factors (physiological and contextual state) mapped to the acoustic features they move.
"Drives in sim" names the current signal; how each of those signals evolves over time is owned by
[SIMULATION_MODEL.md](SIMULATION_MODEL.md). Build status for each feature lives in
[ROADMAP.md](ROADMAP.md).

| Input factor | Drives in sim | Acoustic features |
|---|---|---|
| **Heart rate** (vagal and sympathetic) | `m_FastHR + m_SlowHR` | Systole, diastole, and IBI. Crispness and loudness indirectly. |
| **Contractility** (inotropy: how forcefully the heart squeezes) | `m_Contractility` | S1 amplitude, attack sharpness, crispness, PEP (pre-ejection period) and systole |
| **Preload and filling** (Frank-Starling: a fuller heart squeezes harder on the next beat) | Preceding IBI | S1 amplitude (strong), ejection time and systole (mild) |
| **Breath phase** | `m_RespPhase` | RSA (respiratory sinus arrhythmia). S1 and S2 amplitude and centroid (inspiration = quieter and duller). S2 split. |
| **Respiration rate and depth** | `m_RespRate`, `m_RespDepth` | Magnitude of breath effects above. Breath sounds. |
| **Rhythm type** | `RhythmEngine` | Timing pattern and per-beat morphology (PVC: soft S1, reduced S2, short systole) |
| **Fatigue** | Acute and long-term fatigue | Indirect via HR and contractility. Possible direct dullness. |
| **Auscultation position and body posture** | Needs posture state | S1-to-S2 balance, spectral tilt |
| **Subject age** | Needs age or character parameter | S2 split width, RSA magnitude, S4 presence |
| **Environment** | Spatial audio (future) | Distance falloff, reverb, stereo position |

Each output feature and the factors that drive it:

- **Timing** (IBI, systole, diastole, RSA, PVC coupling and pause): HR, contractility, breath
  phase, and rhythm type.
- **Loudness** (overall level, S1 amplitude, beat-to-beat dynamics; S2 held fixed): contractility,
  preload, and breath phase.
- **Spectral centroid and crispness** (S1 and S2 treated separately, since A2 is genuinely
  higher-pitched than M1): HR, contractility, and breath phase.
- **Attack and decay envelope** (heart sounds are transients, and higher drive currently concentrates
  and shortens the observed S1 envelope): contractility and HR. Onset compression carries the
  drive-dependent change; the post-peak source body is preserved.
- **Extra sound events** (S3 and S4, A2-P2 split, murmurs, breath sounds): breath phase, age, and
  pathology. Mostly deferred.

## Second-order couplings

Subtle but real effects that separate synthetic from recorded. Capture as many as feasible.

1. **Breath phase moves loudness, centroid, and pitch together.** Lung inflation makes each beat
   quieter, duller (a low-pass *muffle*), and slightly lower in *pitch* (`BreathPitchDipDepth`). The
   current pitch magnitude is weakly supported: the ref11 F0 drop is a suspect cross-gap/estimator
   measurement (see [WI-008](work_items/WI-008-spectral-breath-muffle.md)); the muffle direction is
   well-grounded but the pitch dip may shrink or vanish, in
   which case re-tune the constant. The muffle and the pitch dip (if real) are layered, not alternatives. All share the same driver (`m_RespPhase`) and scale with
   the lagged breath depth state (`Simulation::m_RespDepth`), so they are correlated, not independent.
   Depth follows its own early-saturating tidal-volume target (`RespDepthAtVT1` / `RespDepthAtRCP`),
   separate from the late-accelerating respiratory-rate curve; [SIMULATION_MODEL.md](SIMULATION_MODEL.md)
   owns that ventilation partition. The
   loudness attenuation (`BreathAmpDepth`)
   is the *slow* half of the beat-to-beat loudness variation (coupling 12): its depth is set so the
   respiratory swing balances the fast vigor jitter, matching the references' roughly even slow/fast
   split. Because breath depth scales with ventilation, this swing deepens with exertion - large at
   maximal effort, small at a steady cruise - which reproduces the state-dependence references show.
2. **The S2 valve-clap (P2) is a contractility-scaled HF transient, not a broadband brightness.**
   The aortic and pulmonary closures (A2, P2) are sharper transients than the mitral closure (M1).
   The audible "clap" is a brief (~2-5 ms) **HF transient** (~120-200 Hz at tricuspid, ~260 Hz up to
   ~550 Hz at the pulmonary/Erb's site - ref14), energetically tiny next to S2's loud ~50 Hz
   fundamental but very audible because the ear is ~30 dB more sensitive at 300-500 Hz than at 50 Hz.
   The broadband S2 *centroid* is the wrong metric for the clap: it is dominated by that loud
   fundamental, so it can read S2/S1 < 1 (ref8 0.91x, ref14 0.81x) *even when a clap is clearly
   present*. Measured the right way (A-weighted HF band, time-localized), the clap grows with
   contractility (ref14 pulmonary: 220-320 Hz band x1.77 baseline->release) - a forceful contraction
   closes the valves harder. It is also auscultation-site-dependent (strong at pulmonary/tricuspid,
   near-absent on dull mics like ref8), and the site sets its center frequency. Everything *else* about
   S2 is static with drive: the systematic ref-vs-engine pass on a tail-immune A-weighted
   peak-anchored HF ruler (`tools/measure_clap.py`) found S2's broadband brightness and fundamental flat
   with drive and **S1 the sole general drive-brightness responder** (S1's A-weighted HF share
   doubles/triples with drive while S2's broadband stays flat - and flat outright at a no-clap site like
   ref15). So the clap is the *only* S2-with-drive effect worth eventually modeling; the rest stays fixed.
   This deferred P2/clap feature is separate from the
   [operational S2 timing landmark](MEASUREMENT_METHODS.md#preferred-rulers); a distinct P2 onset would be
   a separate future landmark. The annotator's HF fallback is not an anatomical component classifier.
   - **Our source S2 is a faithful real recording** (F0 ~40 Hz, at/just below the low end of the
     references' own S2 fundamentals, ~43-76 Hz across recordings), so do not "raise" its
     pitch/fundamental; that would impose another person's heart onto this one. Caveat: like S1 it is
     boom-heavy (~45% of its energy is sub-40 Hz vs references' ~9-21%). The source-brightness low-cut
     (`SourceHighPassHz`) is therefore applied to S2 as well as S1: at the 40 Hz corner it
     strips S2's sub-20 Hz boom (~20% -> ~3%) while its fundamental **stays put at ~40 Hz** (the peak does
     not move, confirming 40 Hz is S2's genuine fundamental rather than a buried sub-bass mode) - removing
     boom without shifting the fundamental, distinct from "raising" the pitch. Note S2's fundamental sits
     right at the corner, so unlike S1 (76 Hz, ~an octave of headroom) S2 has little room to push the
     corner higher; even post-cut S2 stays boomier than ref8 (20-40 Hz ~39% vs ~10%), so a possible
     future lever is a *higher* S2 corner, traded against thinning that 40 Hz fundamental. The only genuine S2 gap
     otherwise is its soft *attack* (coupling 5), shared with S1 as a source-asset trait.
   - To synthesize the clap: a brief band-limited transient (sum of damped sinusoids, ~2-5 ms
     raised-cosine attack, fast decay) added at S2 onset, amplitude scaled by contractility, mix
     tunable (0 = off), center frequency by site. A harmonic exciter is the wrong tool - it distorts
     the low fundamental (buzzy) instead of adding a clean snap. Do not model any "fade" - the
     centroid fall is emergent from the fundamental growing.
   - Because the clap is site-dependent and auscultation position is not yet simulated, a single
     default would be a guess; it stays deferred (see
     [additional audio realism](ROADMAP.md#additional-audio-realism)). Treat S1 and S2
     spectra independently regardless; never apply one filter or resample ratio to both.
3. **Inspiration brings a correlated package:** an RSA-lengthened cycle, a possible S2 split, and
   quieter and duller beats. These are not independent knobs. They all come from the same in-breath.
4. **Post-pause beat is louder and brighter.** A long diastole means more filling and a more
   forceful, sharper contraction (Frank-Starling). S1 amplitude, brightness, and attack sharpening
   share the per-beat vigor path (`contractility x preload`) described in coupling 5.
5. **S1 attack and post-peak body are separate envelope decisions.** Forceful contraction (high
   contractility + good filling) drives the onset and overall amplitude. The post-peak body preserves the
   source's natural multi-lobe structure and decay rather than forcing a monotonic envelope or adding a
   synthetic continuation.
   - *Faster onset.* Greater drive is modeled as a faster onset and shorter S1, compatible with a steeper
     pressure rise and more abrupt deceleration. Grounded on the within-recording drive pairs
     (ref11 -21.9 ms, ref14 -7.4 ms, level confound
     controlled; [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md)); the source is not slow relative to the
      references. `FindBaselineAttackRegion` computes it from the analytic envelope (pocketfft - the same
      transform `shrlib.env_analytic` uses) at `AttackBuildThreshold` = 0.10, the project's single onset
      definition; `PrepareS1SourceStage` consumes that cached region and compresses the ~9.7 ms build.
      The two band-split numbers cited below (80-200 Hz onset ~2.3 ms, 0-80 Hz rise ~9.8 ms) are
     **excluded from tuning evidence**: a quarter period at 140 Hz is 1.8 ms and at 25 Hz is 10 ms,
     suspiciously like the invalid attack ruler's output on band-limited signals. See the
     [measurement-validity policy](MEASUREMENT_METHODS.md#validity-before-value). The slow part
     is the low-frequency **"whomp" body** (0-80 Hz), and that gradual low
     swell *is* the heart-sound character. So the onset is sped by **time-compressing only the final
      ascent to the peak** and splicing the lead-in and body back on - a faster *gradual*
      swell, F0/whomp preserved (`PrepareS1SourceStage`, `k = 1 + (AttackCompressMax -
     1)*contractility*frankStarlingNorm`). It is high-HR only: the resting/low-HR beat is already
     near-indistinguishable from a real recording (rest k=1, untouched). Drive it by per-beat vigor
     (contractility x preload) so the sharpest beats are occasional ("not every beat"), not uniform.
     Two tempting alternatives do not work: pre-peak gating (an abrupt step = a click, and it guts the
     whomp's swell), and re-trimming the sub-sample to a "sharper" onset (pops - any later start lands
     mid-swing; note the source has **no silent pre-roll**, but two precursor lobes, the second reaching
     55% of peak, before the null at 47 ms where the final ascent begins). And crushing the loudest beats into
     the soft-knee flattens the very peak the onset creates (ref8 S1 crest ~3.6 vs an over-saturated
     render ~2.1), so saturation must stay engine-faithful (no extra drive). The current un-tamed,
     no-tail chain has no meaningful residual softness, reversed-like character, or truncation by ear;
     no additional onset compression is warranted.
   - *Natural body and decay.* The source S1 is not a clean click-then-decay: its post-cut envelope has a
     later maximum roughly 18 ms after the main maximum. That spacing is compatible with normal M1-T1
     separation, but a single monaural source without ECG or simultaneous valve-site channels cannot
     assign the lobe to T1; its roughly 50-60 Hz ringing/interference pattern can produce similar spacing.
     The engine therefore treats the complete sliced source as one mixed-component baseline.

     The engine applies no post-peak tamer or synthetic tail. Preserving the source body is perceptually
     closest to ref8 under matched vigor and respiratory state, with no regression at rest, natural
     breathing, or recovery. This does not imply that real S1 lacks ring-down: reference late-S1 energy
     exists, and the source already carries post-peak structure. A subtractive tamer would remove part of
     that natural body, while a single-mode continuation would replace it with a less representative
     decay.

     A fixed source-relative tail splice is also incompatible with onset compression: compression
     shortens the dry S1 without moving the splice, so the dry source can end before the tail ramp reaches
     full strength at high vigor. A dry-end-relative splice restores coherent geometry but does not
     improve the timbral match. A decaying-ceiling tamer based on box-smoothed sample magnitude is
     unsuitable because its detector is carrier-phase/frequency sensitive and its monotonic-lobe premise
     conflicts with the common multi-lobe reference morphology. `tools/legacy_s1.py` retains these
     alternatives only as explicit counterfactual transforms over the compiled source stage; shipping
     synthesis performs neither stage.

     The 40-80 Hz body-*fraction* gap is a source-spectrum issue rather than a tail-length problem: the
     source whomp is sub-40 Hz-heavy, so a future correction belongs to a source-brightness/EQ pass, not
     to synthetic extension. The source slice still ends before its editing bump, and the high-rate
     `S1SystoleFraction` cap may shorten the dry source without inventing a continuation. (No prominent
     S3 - diastole holds only a small fraction of S1 energy.)
6. **At high heart rate, the systole-to-diastole ratio approaches one,** so the ear loses the
   "lub versus dub" distinction and the sound becomes gallop-like. `S1SystoleFraction` caps S1 inside
   systole and `S2WindowFraction` caps S2 inside the remaining IBI, preserving ejection and diastolic
   gaps. Both caps exceed the fixed source-lobe lengths at rest, so low-rate beats remain unchanged.
   Their current setpoints are unaudited; if high-rate dry S1 shortening becomes audible, treat the cap
   as its own timing decision.
7. **S3 and S4 are low-frequency** (about 20 to 50 Hz, below S1). S3 is an early-diastole thud
   common in young or athletic hearts. S4 is a late-diastole thud in a stiff or older ventricle.
   At high heart rates they can merge into a summation gallop.
8. **RSA magnitude shrinks with exertion** (modeled) and with age (not modeled).
9. **Stethoscope recordings are inherently low-pass** (about 20 to 150 Hz dominant). This is part
   of the familiar heart-sound character we want to match, distinct from the physiological
   frequency shift with drive.
10. **Loudness also depends on body composition** (chest-wall thickness in particular). This is a
    per-character constant, not physiological state. A possible fitness proxy if needed.
11. **Murmurs scale with flow velocity and turbulence,** which tracks cardiac output (HR times
    stroke volume) when a valve lesion is present. Pathology, and currently out of scope.
12. **Beats vary beat-to-beat, and the variation grows with drive.** A per-beat vigor jitter
    (`Constants.hpp: VigorJitterScale`, applied in `RhythmEngine`) perturbs the instantaneous
    contractility around the mean sympathetic drive, so one driver moves S1 loudness, brightness
    (via onset shaping), and attack together - the co-varying "liveliness" references show, not three independent
    random knobs. The beat-to-beat loudness variation has two separable components. The **slow** one is
    the respiratory swing: it tracks the breath cycle (a much longer period than beat-to-beat) and is
    carried by the breath amplitude/muffle modulation (coupling 1), so its depth scales with the lagged
    ventilation state rather than instantaneous exertion. The **fast** one is contractile: in the references it is decoupled from timing and
    filling (per-beat S1 amplitude is uncorrelated with the preceding interval, and IBI itself barely
    varies at high rate), it is S1-specific (S2 amplitude is preserved on a weak beat, ruling out a
    whole-beat coupling artifact), and its distribution is near-Gaussian and white (no heavy tails or
    beat-to-beat correlation). It is therefore carried on instantaneous contractility rather than
    Frank-Starling, and jittered Gaussian-white. Its magnitude scales with contractility, so it grows
    with sympathetic/adrenergic drive - a steady-exercise state varies about half as much as an excited
    one - which also ties the beat-to-beat liveliness to fight-or-flight arousal for free. Because the
    jitter rides contractility and can push an individual beat's instantaneous force above the mean
    drive, it is unclipped *at the mean* - above-average beats survive rather than clipping at unity - and
    the transducer soft-knee (the operating point, in the Signal-chain architecture above) limits the
    loudest, with headroom so those upward excursions are not crushed. The extreme upper *tail*, however,
    is bounded (`VigorJitterMaxSigma`): the rendered output is physically limited regardless of the
    physiological tail (a transducer emits no square wave on a forceful beat), and an unbounded draw into a
    hard ceiling eventually flat-tops a beat into an audible over-loud/harsh artifact (offline: flat-top
    onset ~3.4sd). The clamp sits just above ref13's loudest observed beat (+2.35sd) - the smallest bound
    that spares demonstrated real variation - and, being far out, leaves the jitter's calibrated body and
    variance essentially untouched (loudness CV changes < 0.2 pp).

## Contractility Keystone

Several high-value audio features share one driving signal: contractility (how forcefully the
heart squeezes), which captures the effects of sympathetic nervous drive and circulating
catecholamines (stress hormones like adrenaline).

The features it unlocks:

- Systole hysteresis after exercise: PEP (the most contractility-sensitive interval) shortens with
  sympathetic drive and lingers after exertion stops.
- S1 amplitude beyond Frank-Starling: the Bowditch force-frequency effect and beta-adrenergic
  inotropy (the heart squeezing harder in response to faster rate and catecholamine drive).
- Attack sharpness (dP/dt).
- Spectral crispness with drive.

The signal is driven by normalized exertion (a metabolic and sympathetic proxy) plus a
contribution from circulating adrenaline for emotional or acute drive. It rises on a roughly
20-second onset time constant and decays on a roughly 120-second time constant, matching
circulating catecholamine clearance at about two minutes. The slow decay is the hysteresis knob
that makes post-exercise beats linger as forceful even as HR recovers. Full implementation spec in
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md).

The current signal is an **audio-only scalar** exposed through
`PhysiologySnapshot::Contractility`. The scalar does not feed back into HR, but its v1 inputs have a
known double route: adrenaline first enters the exertion target and HR, then enters
`ContractilityTarget` again as a direct adrenergic term. Contractility v2 will replace that routing with
separated vagal, noradrenergic, and epinephrine drivers feeding HR, contractility, and a fight-or-flight
gameplay effect without changing consumers. The current
bi-exponential HR model conflates
noradrenaline (roughly one-minute timescale) and epinephrine (roughly two-minute timescale) in one
slow component and models only chronotropy (the heart-rate effect). Their deferred separation is described in
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#deferred-driver-separation).
