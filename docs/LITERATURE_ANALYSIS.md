# Literature Analysis

Published-source evidence behind the model's `[physio]` constants: what the literature reports, the
bracket it supports, and how the shipped value sits inside it.
[REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md) is the equivalent for our own recordings.

An entry names the constants it backs, states the bracket with units, cites its sources, and says where
the model departs from them. A bracket is a defensible range rather than a fit: it justifies scale and
ordering, and a value outside it needs an argument. Unlike the measurement side, whose bindings
`tools/data_citations.toml` checks against recomputable leaves in `references/measurements.json`, a
published figure is frozen and carries no such binding.

Figures are marked **[primary]** where the paper itself was read and **[secondary]** where the figure
comes from an abstract, a review's summary, or a database description. A `[secondary]` figure can
establish a bracket's scale; confirm it against the primary source before treating it as a bound.

## Aerobic capacity endpoints

Backs `FitnessBaseMets`, `FitnessEliteMets`, the default of `SimulationSettings::FitnessMaxMets`,
`MaxRestingHR`, and the derived `FitnessAbsoluteMin`.
Capacity is in METs, one MET being 3.5 mL/kg/min of oxygen uptake.

Population reference values, from the Fitness Registry and the Importance of Exercise National
Database (FRIEND), 50th percentile **[secondary]**:

| Age | Men (mL/kg/min) | Women (mL/kg/min) |
| --- | --- | --- |
| 20-29 | 49.5 | 40.6 |
| 70-79 | 30.8 | 25.0 |

VO2max declines about 9% per decade across that span **[secondary]**. Soldiers tested under load
carriage measure roughly 47-57 mL/kg/min **[secondary]**; elite endurance athletes exceed 70, with
cross-country skiers reaching 80-90 **[secondary]**. About 17.5 mL/kg/min (5 MET) is the capacity
conventionally associated with independent living **[secondary]**.

`FitnessBaseMets` at 24.5 mL/kg/min is close to the median woman in her seventies - low, but above the
independence threshold, which is the right character for a floor representing a healthy but fully
detrained adult. `FitnessEliteMets` at 70 is the elite-endurance line, which is why it anchors the
fitness normalization; it is defensible as a ceiling too, though the archetype the mod serves is nearer
the soldier band, so the per-character `FitnessMaxMets` should not default to the maximum.

Two consequences of the endpoints are not evident from the values:

- `FitnessAbsoluteMin` derives from `MaxRestingHR` and `RestingHRSlope` rather than from a capacity
  argument, and evaluates to about 3.3 MET (11.7 mL/kg/min), below the independence threshold. It is a
  division guard for the target-heart-rate computation, not a physiological claim.
- Because fitness decays toward `FitnessBaseMets`, no character can have a capacity ceiling below it.
  The lowest expressible character is a sedentary but healthy adult; a chronically ill one is outside
  the model's range unless `FitnessBaseMets` itself becomes per-character. That limit, and the related
  one that resting heart rate is derived from fitness so "fit but tachycardic" cannot be expressed, are
  owned by [#23](https://github.com/GrazedAnkle/SHR-SKSE64/issues/23).

## Fatigue reduction of aerobic capacity

Backs `AcuteFatigueMaxFraction` and `LongTermFatigueMaxFraction`.

The relevant work treats resilience to physiological decline during prolonged exercise as a property in
its own right - durability, proposed as a fourth determinant of endurance performance alongside
VO2max, threshold, and efficiency. Its measurements are expressed as a percentage of the individual's
own capacity:

| Insult | Capacity loss | Confidence |
| --- | --- | --- |
| 90 min running at lactate threshold | ~6% VO2peak; speed at threshold 12.8 to 12.1 km/h (~5.5%) | [primary] |
| 120 min running | 7.1% VO2max | [secondary] |
| 2 h cycling | power at first ventilatory threshold -6 +/- 7%; 5 min time trial -9 +/- 10%; VO2peak not significant | [primary] |
| Prolonged cycling | critical power ~10% mean, inter-individual range 0.4-32% | [secondary] |
| 3 week overload block (functional overreaching) | incremental test performance -9.0 +/- 2.1%; VO2max reduced only in the overreached subgroup | [secondary] |

That supports roughly **5-15% for acute fatigue**, with a tail toward 30% at glycogen depletion, and a
further **5-10% for the chronic layer**.

### The reduction is proportional, and to raw fitness

Every figure above is a fraction of the subject's own capacity, never an absolute oxygen-uptake
decrement. Expressing the maxima as fractions of a *global* ceiling is therefore the wrong shape: it
denominates a deconditioned character's fatigue in an elite character's capacity units, driving
capacity losses far outside any published bracket and, at the low end, into the `FitnessAbsoluteMin`
clamp.

The proportionality is to raw fitness, not `EffectiveFitness`, which is already capacity minus fatigue
and would place fatigue inside its own target. This is the same reasoning that makes
`UpdateAcuteFatigue` normalize exertion by raw fitness.

### Proportional maxima and absolute-MET exertion

The literature doses at a *relative* anchor - 90 minutes at the subject's own lactate threshold, two
hours at their own moderate intensity - so a fitter subject performs more absolute work by
construction. The model doses in absolute METs, because Skyrim fixes movement speed for every character
and the metabolic cost of covering ground is the same for all of them.

These act at different points in the chain rather than conflicting. The literature constrains the
response function: at matched relative intensity, capacity loss is a roughly constant fraction of one's
own capacity. The model converts an absolute dose to relative intensity first, then applies that
response, so equilibrium fatigue is a proportional maximum times normalized intensity and matched
intensity yields a matched fraction.

Under absolute dosing the model then predicts that a less fit character loses more relative capacity at
the same task. The durability protocols do not test that prediction, but untrained individuals reach
lactate threshold near 50-60% of VO2max against 75-85% in the trained **[secondary]**, so identical
absolute work sits far deeper into their reserve. The same mechanism is why a low-capacity character
feels fragile without any additional fragility term: `RunningMets` is a modest fraction of an athlete's
capacity and the whole of a sedentary character's.

### Limitation: the fatigue driver saturates

`UpdateAcuteFatigue` clamps normalized exertion to at most 1, while `NormalizedExertion`, which drives
heart rate, is deliberately unbounded above. Heart rate keeps responding to supra-capacity effort while
fatigue stops accruing, so a low-capacity character reaches identical fatigue from jogging and from
sprinting. This is independent of the magnitude bracket above;
[#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27) owns the saturation policy, because the
driver model it replaces has to settle one convention for both.

## Fatigue resistance and aerobic capacity

Cross-sectionally the association is real. Trained individuals sustain effort longer at the same
relative intensity, show faster oxygen-uptake kinetics - a time constant near 28-29 s against 59-64 s
in untrained subjects **[secondary]** - and recover better between efforts, which a review of aerobic
fitness and recovery attributes to a larger aerobic contribution, improved lactate removal, and
enhanced phosphocreatine resynthesis **[primary, qualitative]**. That review reports the relationship
as strongest for the fraction of VO2max at 4 mmol/L blood lactate rather than for VO2max itself, and
finds the phosphocreatine association depends on the active muscle mass.

Longitudinally it breaks down. Across a ten-week training study, durability improved under both low-
and high-intensity training - energy-expenditure drift fell from about 6.4% to 4.0% and from 6.5% to
3.0%, and the onset of drift was postponed by roughly 25-29 minutes - yet change in VO2max was not
associated with change in any drift, which the authors read as evidence of distinct mechanisms
**[primary]**.

The model carries one capacity scalar standing in for training status, and training status predicts
both. Coupling fatigue resistance to fitness is the cross-sectional relation, which is the one a
single-scalar model can express. The liberty taken is that within-subject changes in capacity and in
resilience move together, which the longitudinal evidence says they need not.

## Resting heart rate and the sleeping reduction

Backs `BaseRestingHR` and `SleepFraction`, completing the resting-heart-rate line whose fitness endpoints
the aerobic-capacity section above already covers.

The Fenland Study measured resting heart rate in 10,865 UK adults aged 29 to 65 in three conditions
within one cohort - seated 67, supine 64, and asleep 57 beats/min **[secondary]**. Separately, sedentary
young adults measure 82.2 +/- 12.7 beats/min against 76.4 +/- 10.9 in physically active ones
**[secondary]**, and 60 to 100 beats/min is the conventional clinical normal range.

`BaseRestingHR` is the rate at `FitnessBaseMets`, the detrained floor, rather than at the population
mean, so the cohort's seated 67 is a lower bound rather than a target and the sedentary figure is the
nearer anchor. That supports roughly **75-85 beats/min**, and the shipped value sits inside it.

`SleepFraction` has an unusually direct anchor: the seated-to-asleep ratio in the Fenland cohort is
57/67, or **0.85**. This is a better figure than the commonly quoted 10-30% nocturnal dip because it is
one cohort measured under both conditions rather than two ranges compared across studies. The deeper
published dips are sleep-stage specific and REM returns near the waking rate; the model has no sleep
architecture, so a whole-night ratio is the right shape for it.

### The resting line is anchored on its endpoints, not on the published slope

The same study reports the fitness association as -0.26 to -0.31 mL/kg/min per beat, which inverts to
about 11 to 13 beats/min per MET. `RestingHRSlope` is far shallower, and the discrepancy is a property
of the ruler rather than an error: a regression fitted across a population whose resting rates span a
few tens of beats cannot be extrapolated across the model's full capacity range, where it would drive
an elite character's resting rate below zero.

What supports the line is its endpoints. `EffectiveRestingHR` evaluates to about 40 beats/min at
`FitnessEliteMets`, which is the rate reported for highly trained endurance athletes, and the shipped
`BaseRestingHR` at the other end. A line through those two points has the shipped slope. `RestingHRSlope`
and `MaxRestingHR` themselves belong to the long-term-fitness group and are bracketed with it.

## Heart-rate response kinetics

Backs `FastOnsetTauSedentary`, `FastOnsetTauElite`, `SlowOnsetTau`, `FastRecoveryTauSedentary`,
`FastRecoveryTauElite`, `SlowRecoveryTau`, and `HRFastFraction`.

Onset **[all secondary]**:

| Cohort and condition | Reported |
| --- | --- |
| Untrained, light step, mono-exponential | tau 27 +/- 21 s |
| Endurance-trained, same absolute load | tau 8 +/- 3 s |
| Endurance-trained at 45% / 60% VO2max | tau 12 +/- 4 s / 22 +/- 6 s |
| Trained vs untrained, incremental protocol | half-time 24.1 +/- 3.4 s vs 47.1 +/- 4.1 s |

Recovery **[all secondary]**:

| Cohort and condition | Reported |
| --- | --- |
| Recovery from 21% / 43% / 65% VO2max, six sedentary men | tau 13.6 +/- 1.6 / 32.7 +/- 5.6 / 55.8 +/- 8.1 s |
| Parasympathetic reactivation, two-constant fit, eleven subjects | tau 44 +/- 37 s |
| Sympathetic withdrawal, same fit | tau 65 +/- 56 s |

### Comparing these to a two-component model

Every figure above is a fit to the *whole* heart-rate response. The model splits that response between a
fast and a slow term, so no published tau brackets `FastOnsetTauSedentary` or any of its siblings
directly. The comparable quantity is the model's composite: the half-time of `HRFastFraction` weighted
against its complement, converted to an equivalent single time constant.

Composite onset is about **30 s** for a character at `FitnessBaseMets` and **18 s** at
`FitnessEliteMets`. The sedentary end sits inside the untrained bracket. The elite end is slower than
trained subjects at light absolute load but matches them at 60% VO2max, which is the intensity a fight
actually represents, so the departure is in the protocol rather than in the value. The sedentary-to-elite
ratio of 2 is separately corroborated: the incremental study's half-times give 47.1/24.1, or 1.96, from
an independent cohort and protocol.

Composite recovery is about **89 s** sedentary and **53 s** elite. The elite end lands just inside the
submaximal study's hardest condition; the sedentary end is slower than every intensity it measured, and
`SlowRecoveryTau` contributes most of that excess - taken alone it sits at roughly two standard
deviations above the sympathetic-withdrawal figure. The model departs knowingly: the slow tail also
carries the post-exercise separation between heart rate and beat character recorded in
[REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md), which a tau inside the published bracket would not
sustain. [#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27) owns the re-derivation and
should treat this as a known departure rather than a free parameter.

### Recovery varies with intensity, and the model varies it with fitness

The submaximal study's three taus are ordered by exercise intensity, not by subject fitness - they come
from one cohort of six. `FastRecoveryTauSedentary` and `FastRecoveryTauElite` vary on fitness instead,
which the literature supports directionally through parasympathetic reactivation but does not bracket.
The consequence is that this table cannot constrain the shipped values even where the numbers overlap:
it measures a different axis. Recording that is the point, because
[#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27) re-derives these constants and
[#26](https://github.com/GrazedAnkle/SHR-SKSE64/issues/26) explicitly excludes intensity-dependence from
its own scope, so the gap would otherwise pass unnamed between them.

### First-minute recovery is an acceptance criterion, not a bracket

The clinical first-minute figures - normal near 21-28 beats/min, and below 12 an established mortality
predictor - are tempting to convert into a time constant and use as a bracket. They do not convert
cleanly. Those protocols use an active cool-down, so the recovery asymptote is a walking rate rather than
pre-exercise rest, and the tau implied by a given first-minute drop moves by roughly half depending on
which asymptote is assumed. The submaximal figures above, which decay toward rest, are the bracket;
first-minute recovery stays what [#26](https://github.com/GrazedAnkle/SHR-SKSE64/issues/26) already makes
it, a criterion applied to the model's own trajectory.

### No published bracket applies to the fast fraction

`HRFastFraction` sets how the response amplitude divides between the two components. The literature
reports the two *time constants* consistently and the amplitude split hardly at all: recovery work
attributes the fast phase to parasympathetic reactivation and the slow phase to sympathetic withdrawal
without stating a stable proportion, and onset work fits phase amplitudes per protocol rather than
reporting a population figure. No defensible published range was found.

It is derivable rather than unbracketable. First-minute recovery is a closed form in the fast fraction
and the two recovery taus, so pinning the taus determines the fraction against a first-minute target. The
constraint is that one scalar cannot fix three parameters, which makes the fraction a consequence of the
tau decision rather than an independent choice - and therefore work for
[#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27), after it settles the taus. As shipped, the
constants predict a first-minute recovery near half the reserve, above the clinical normal band.

`SlowOnsetTau` is the same case for a different reason. Published work reports the heart-rate slow
component as a drift *slope* - about 0.55 to 2.2 beats/min per minute across the moderate, heavy, and
severe domains **[secondary]** - rather than as a time constant, because it is measured as upward drift
during sustained work rather than as an approach to a step. Converting a slope and an amplitude into a
time constant is mechanical, so this is a derivation that has not been done rather than evidence that
does not exist.

## The absolute heart-rate ceiling

Backs `HRFormulaCeiling`.

The constant's role is narrower than its name suggests. `ComputeTargetHeartRate` scales between the
effective resting rate and the per-character `SimulationSettings::MaximumHeartRate`, and applies
`HRFormulaCeiling` as a final cap above that. It binds only because `NormalizedExertion` is deliberately
unbounded above, so supra-capacity effort can drive the target past the character's own maximum. It is a
guard against a non-physiological heart rate, not the maximum any character reaches in play.

Read that way it is defensible: the meta-analytic age relation `208 - 0.7 x age`, from 351 studies and
18,712 subjects, predicts about 194 beats/min at age 20 with a standard deviation near 10 **[secondary]**,
which places the shipped value at the top of the plausible human range. The supported bracket is roughly
**200-220 beats/min** and the value sits at its ceiling, which is the correct end for a guard.

The departure is documentary. The value was inherited from the `220 - age` heuristic, which the same
meta-analysis shows overestimates maximal heart rate in young adults by more than 12 beats/min and which
its authors replaced. Nothing needs to change while the constant is a flat cap, because 220 is defensible
on its own as an absolute ceiling. If it is ever re-derived as an age relation, the intercept to derive
it from is 208, not 220.

## Catecholamine clearance

Backs `AdrenalineHalfLife`.

Circulating epinephrine and norepinephrine have plasma half-lives of about **1-3 minutes**
**[secondary]**, with metabolic clearance of 2 to 6 L/min and degradation in liver and kidney by
catechol-O-methyltransferase and monoamine oxidase. Clearance falls with age and rises with body mass,
neither of which the model expresses. The shipped value sits mid-bracket.

Two consequences matter more than the value. Clearance is the one part of the current adrenaline path
that is well anchored, so [#26](https://github.com/GrazedAnkle/SHR-SKSE64/issues/26) is right that the
defect it records must not be absorbed by retuning this constant. And the recovery evidence above
separates the clocks: heart rate falls with time constants of tens of seconds while norepinephrine decays
with 87 to 101 s in the same subjects, which is the measurement
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#why-the-v1-fusion-is-wrong-not-merely-unseparated) rests its
driver-separation argument on.

## Coverage and enforcement

Bracket coverage stays **prose in this document**, and is **not machine-checked** for now.
[#22](https://github.com/GrazedAnkle/SHR-SKSE64/issues/22) owns the decision; this section records it.

The measurement side splits across two files because it has to: `tools/data_citations.toml` binds a
claim to a recomputable leaf in `references/measurements.json`, and a machine needs a machine-readable
target. Published brackets have no equivalent target. The figure is frozen, and the bracket is an
argument about which figures transfer to a game model - the heart-rate entries above turn on a regression
that cannot be extrapolated, a protocol mismatch, and an axis mismatch, none of which survive reduction
to a low and a high bound.

One assertion is genuinely mechanical and worth having: that a shipped value lies inside its declared
bracket. That is the check which would have caught the acute-fatigue maximum against its 5-15% bracket.
Building it now would mean a second file declaring bounds for the covered constants and exemptions for
the rest, and while the uncovered majority dominates, that file is mostly exemption bookkeeping whose
entries decay unread.

The trigger for revisiting is therefore coverage, not effort: once bracketed constants outnumber
unbracketed ones, add a bounds table checked in both directions - every entry has a prose section here,
and every prose section has an entry. Until then a bracket without a section is caught by review, which
is the same mechanism that has to read the argument anyway.

## Known gaps

- **Fatigue time constants are unbracketed.** `AcuteFatigueGainTau`, `AcuteFatigueDecayTau`,
  `LongTermFatigueGainTau`, `LongTermFatigueDecayTau`, and `SleepRecoveryRate` have no entry above. The
  durability literature measures decline, not clearance, so it does not transfer.
  [#28](https://github.com/GrazedAnkle/SHR-SKSE64/issues/28) owns them.
- **The remaining `[physio]` groups have no entry.** Activity intensities, respiration, respiratory sinus
  arrhythmia, premature-ventricular-contraction rhythm, and the rest of the long-term-fitness group are
  uncovered. Activity intensities are the next slice and the least contentious, since the Compendium of
  Physical Activities publishes MET values per activity directly.
  [#22](https://github.com/GrazedAnkle/SHR-SKSE64/issues/22) owns closing the backlog.
- **Two heart-rate constants are derivable but underived.** `HRFastFraction` and `SlowOnsetTau` have
  no published bracket, and the kinetics section above gives a derivation route for each.

## Sources

- Jones AM. The fourth dimension: physiological resilience as an independent determinant of endurance
  exercise performance. *J Physiol*, 2024. <https://physoc.onlinelibrary.wiley.com/doi/10.1113/JP284205>
- Durability of the moderate-to-heavy-intensity transition is related to the effects of prolonged
  exercise on severe-intensity performance. <https://pmc.ncbi.nlm.nih.gov/articles/PMC11322397/>
- Durability of parameters associated with endurance running in marathoners.
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC12547624/>
- Durability is improved by both low and high intensity endurance training. *Front Physiol*, 2023.
  <https://www.frontiersin.org/journals/physiology/articles/10.3389/fphys.2023.1128111/full>
- Prolonged cycling reduces power output at the moderate-to-heavy intensity transition.
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC9488873/>
- Le Meur Y et al. Maximal exercise limitation in functionally overreached triathletes: role of cardiac
  adrenergic stimulation. *J Appl Physiol*, 2014. <https://pubmed.ncbi.nlm.nih.gov/24925979/>
- Tomlin DL, Wenger HA. The relationship between aerobic fitness and recovery from high intensity
  intermittent exercise. *Sports Med*, 2001. <https://pubmed.ncbi.nlm.nih.gov/11219498/>
- Kaminsky LA et al. Reference standards for cardiorespiratory fitness measured with cardiopulmonary
  exercise testing (FRIEND). *Mayo Clin Proc*.
  <https://www.mayoclinicproceedings.org/article/S0025-6196(15)00642-4/pdf>
- Metabolic costs of military load carriage over complex terrain. *Mil Med*, 2018.
  <https://academic.oup.com/milmed/article/183/9-10/e357/5025891>
- Oxygen uptake kinetics and time to exhaustion in cycling and running: a comparison between trained and
  untrained subjects. <https://pubmed.ncbi.nlm.nih.gov/16026035/>
- Gonzales TI et al. Resting heart rate is a population-level biomarker of cardiorespiratory fitness: the
  Fenland Study. *PLOS One*, 2023.
  <https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0285272>
- Anticipatory heart rate before moderate- and vigorous-intensity exercise among sedentary and physically
  active young adult males. <https://pmc.ncbi.nlm.nih.gov/articles/PMC10757437/>
- Trounson KM et al. Light exercise heart rate on-kinetics: a comparison of data fitted with sigmoidal and
  exponential functions and the impact of fitness and exercise intensity. *Physiol Rep*, 2017.
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC5492202/>
- Kinetics of heart rate responses to exercise. *J Sports Sci*, 1988.
  <https://www.tandfonline.com/doi/abs/10.1080/02640418808729792>
- Plasma norepinephrine and heart rate dynamics during recovery from submaximal exercise in man.
  *Eur J Appl Physiol*. <https://pubmed.ncbi.nlm.nih.gov/2767070/>
- Assessing autonomic function by analysis of heart rate recovery from exercise in healthy subjects.
  *J Appl Physiol*, 2004. <https://pubmed.ncbi.nlm.nih.gov/15219511/>
- Pecanha T et al. Heart rate recovery: autonomic determinants, methods of assessment and association with
  mortality and cardiovascular diseases. *Clin Physiol Funct Imaging*, 2014.
  <https://onlinelibrary.wiley.com/doi/10.1111/cpf.12102>
- Teso M et al. Predicting heart rate slow component dynamics: a model across exercise intensities, age,
  and sex. *Sports (Basel)*, 2025. <https://pmc.ncbi.nlm.nih.gov/articles/PMC11860534/>
- Tanaka H, Monahan KD, Seals DR. Age-predicted maximal heart rate revisited. *J Am Coll Cardiol*, 2001.
  <https://www.jacc.org/doi/10.1016/S0735-1097(00)01054-8>
- Epinephrine. *StatPearls*. <https://www.ncbi.nlm.nih.gov/books/NBK482160/>
