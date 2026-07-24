# Glossary

Plain-language definitions of any jargon or technical terminology used in the project.

## Cardiac cycle and heart sounds

- **Systole** - the phase where the heart contracts and pumps blood out. In audio terms, the gap
  between S1 and the S2.
- **Diastole** - the phase where the heart relaxes and refills. The (usually longer) gap between
  S2 from beat 1, and S1 from the following beat 2.
- **S1** - the first heart sound, made by the mitral and tricuspid valves closing as the
  contraction begins.
- **S2** - the second heart sound, made by the aortic and pulmonary valves closing as the
  contraction ends. Has two parts, A2 (aortic) and P2 (pulmonary).
- **S3, S4** - faint extra sounds. S3 is an early-diastole thud common in young or athletic
  hearts. S4 is a late-diastole thud associated with a stiff (often older) heart.
- **A2 / P2 split** - on a deep breath in, the aortic and pulmonary valves close slightly apart,
  so the "dub" briefly becomes "dub-b". Usually collapses to a single sound on breathing out.
- **IBI (inter-beat interval)** - the time from one beat to the next. Typically measured in ms.
  60 * 1000 / IBI(ms) = heart rate.

## Systolic time intervals (spacing between S1 and S2)

- **PEP (pre-ejection period)** - the brief delay between the heart's electrical trigger and blood
  actually leaving it (the heart building up pressure). Shortens sharply when the heart is driven
  hard. The most contractility-sensitive interval.
- **LVET (left ventricular ejection time)** - how long blood is actually flowing out. Shrinks as
  heart rate rises and as the amount pumped per beat falls.
- **QS2 (electromechanical systole)** - the full audible S1-to-S2 interval - PEP plus LVET. This is
  what we synthesize as the gap between "lub" and "dub" (not LVET alone).

## Drive, contractility, and the nervous system

- **Contractility (inotropy)** - how forcefully the heart squeezes, independent of how fast it
  beats. Higher contractility means a louder, snappier, higher-pitched beat with a shorter PEP.
- **Frank-Starling mechanism** - a fuller heart (more filling time before a beat) squeezes harder
  the next beat. This is why a beat after a long pause is louder.
- **Bowditch (force-frequency) effect** - a faster heart rate also raises contractility on its own.
- **Stroke volume** - the amount of blood pumped per beat. **Preload** is how full the heart is
  before a beat, and **afterload** is the pressure it pumps against.
- **Vagal / parasympathetic** - the "brake" nervous system. This is fast-acting. Its return after
  exercise is what causes the quick early drop in heart rate.
- **Sympathetic** - the "accelerator" nervous system. Slower-acting and lingers for minutes after
  exercise.
- **Catecholamines** - stress hormones such as adrenaline/epinephrine, noradrenaline/norepinephrine
  that raise both heart rate and contractility. Clear from the blood over roughly two minutes.
- **RSA (respiratory sinus arrhythmia)** - heart rate naturally speeds up on breathing in and
  slows on breathing out. Fades during hard exercise.
- **Diving reflex** - breath-holding (as in swimming) tends to slow the heart (bradycardia).
- **Bradycardia / tachycardia** - abnormally slow / fast heart rate.

## Arrhythmia (irregular beats)

- **PVC (premature ventricular contraction)** - an early, extra beat from the lower heart, usually
  felt as a "skip". Followed by a longer-than-normal pause.
- **Coupling interval** - how early the PVC lands, as an fraction of a normal beat's IBI. Lower
  means more premature.
- **Compensatory pause** - the long gap after a PVC, so the next normal beat lands back on schedule.
- **Perfusing vs non-perfusing** - a PVC that ejects enough blood to make an audible "dub"
  (perfusing) versus one too early to eject, which has no "dub" (non-perfusing).
- **R-on-T** - a dangerously early PVC landing on the prior beat's recovery, which can trigger
  worse rhythms.
- **Bigeminy** - a sustained pattern of every other beat being a PVC.
- **NSVT (non-sustained ventricular tachycardia)** - a short run of several very fast beats.

## Exertion and fitness

- **METs** - a unit of physical effort (1 MET is resting, sprinting is roughly 15). Drives the
  simulation's exertion level.
- **VO2max** - a measure of aerobic fitness. Can be expressed in METs.
- **Tidal volume** - how deep a breath is. Rises with exertion, which makes the breath effect on
  the heart sound more pronounced.
- **VT1 / ventilatory threshold** - the first exercise threshold, where ventilation begins rising
  faster than oxygen consumption. Tidal volume supplies most of the ventilation rise below it.
- **RCP / VT2 (respiratory compensation point)** - the second threshold during heavy exercise, where
  breathing rate accelerates to compensate for metabolic acidosis.

## Auscultation (listening positions)

- **Mitral, tricuspid, aortic, Erb's/pulmonary** - standard chest positions a stethoscope is held.
  Each emphasizes different valves, so the S1-to-S2 balance and tone differ by position.

## Audio / DSP

- **Centroid (spectral centroid)** - the "center of mass" of a sound's frequencies. This can act as
  a rough proxy for how bright versus dull it sounds. Our analysis (`tools/shrlib.py`) weights it by
  magnitude, not power: power over-weights the loud low fundamental and masks the brightness shifts we
  track. Absolute centroids are mic-dependent and only compare within one recording.
- **A-weighting** - a frequency curve approximating how loud the ear actually perceives a
  sound. Important here because heart sounds are low-frequency, where the ear is far less sensitive,
  so perceived loudness is not the same as raw amplitude.
- **Formant** - a resonant emphasis in a sound's spectrum.
- **Low-pass / high-shelf / one-pole** - filter types. A low-pass rolls off high frequencies (a
  "muffle"). A high-shelf lifts the highs (brightens). One-pole is the simplest, non-ringing form.
- **Soft-knee (limiting)** - gentle compression that rounds off the loudest peaks instead of
  hard-clipping them, avoiding the harshness of hard clipping. Used here as a clip-prevention limiter
  (see SYNTHESIS_MODEL.md), not to add audible saturation.
- **Resampling** - playing a sample faster or slower, which shifts its pitch and changes its length.
  Used for the breath pitch-dip and PVC dulling. (Contractility brightening is done by envelope shaping
  - onset compression - not resampling, which would shift the fundamental and sound sped-up rather than
  forceful.)
- **Resample pitch-dip vs low-pass muffle** - two different ways to dull a sound: resampling lowers
  the whole pitch, while a low-pass removes only the highs and keeps the pitch.
