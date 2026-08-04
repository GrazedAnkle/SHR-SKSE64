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

`FitnessBaseMets` at 24.5 mL/kg/min is close to the median woman in her seventies — low, but above the
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
its own right — durability, proposed as a fourth determinant of endurance performance alongside
VO2max, threshold, and efficiency. Its measurements are expressed as a percentage of the individual's
own capacity:

| Insult | Capacity loss | Confidence |
| --- | --- | --- |
| 90 min running at lactate threshold | ~6% VO2peak; speed at threshold 12.8 to 12.1 km/h (~5.5%) | [primary] |
| 120 min running | 7.1% VO2max | [secondary] |
| 2 h cycling | power at first ventilatory threshold -6 ± 7%; 5 min time trial -9 ± 10%; VO2peak not significant | [primary] |
| Prolonged cycling | critical power ~10% mean, inter-individual range 0.4-32% | [secondary] |
| 3 week overload block (functional overreaching) | incremental test performance -9.0 ± 2.1%; VO2max reduced only in the overreached subgroup | [secondary] |

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

The literature doses at a *relative* anchor — 90 minutes at the subject's own lactate threshold, two
hours at their own moderate intensity — so a fitter subject performs more absolute work by
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
[WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15) owns the saturation policy.

## Fatigue resistance and aerobic capacity

Cross-sectionally the association is real. Trained individuals sustain effort longer at the same
relative intensity, show faster oxygen-uptake kinetics — a time constant near 28-29 s against 59-64 s
in untrained subjects **[secondary]** — and recover better between efforts, which a review of aerobic
fitness and recovery attributes to a larger aerobic contribution, improved lactate removal, and
enhanced phosphocreatine resynthesis **[primary, qualitative]**. That review reports the relationship
as strongest for the fraction of VO2max at 4 mmol/L blood lactate rather than for VO2max itself, and
finds the phosphocreatine association depends on the active muscle mass.

Longitudinally it breaks down. Across a ten-week training study, durability improved under both low-
and high-intensity training — energy-expenditure drift fell from about 6.4% to 4.0% and from 6.5% to
3.0%, and the onset of drift was postponed by roughly 25-29 minutes — yet change in VO2max was not
associated with change in any drift, which the authors read as evidence of distinct mechanisms
**[primary]**.

The model carries one capacity scalar standing in for training status, and training status predicts
both. Coupling fatigue resistance to fitness is the cross-sectional relation, which is the one a
single-scalar model can express. The liberty taken is that within-subject changes in capacity and in
resilience move together, which the longitudinal evidence says they need not.

## Known gaps

- **Fatigue time constants are unbracketed.** `AcuteFatigueGainTau`, `AcuteFatigueDecayTau`,
  `LongTermFatigueGainTau`, `LongTermFatigueDecayTau`, and `SleepRecoveryRate` have no entry above. The
  durability literature measures decline, not clearance, so it does not transfer.
  [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15) owns them.
- **Most `[physio]` constants have no entry.** Heart-rate dynamics, activity intensities, respiration,
  respiratory sinus arrhythmia, and premature-ventricular-contraction rhythm are uncovered. Closing
  that backlog, and deciding whether coverage should be machine-checked the way
  `tools/data_citations.toml` checks the measurement side, is owned by
  [#22](https://github.com/GrazedAnkle/SHR-SKSE64/issues/22).

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
