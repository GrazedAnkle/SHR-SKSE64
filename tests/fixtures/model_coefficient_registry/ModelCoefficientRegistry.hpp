#define SHR_SIMULATION_MODEL_COEFFICIENTS(X) \
    X(int, Alpha)

#define SHR_RHYTHM_MODEL_COEFFICIENTS(X) \
    X(float, Alpha)

#define SHR_MODEL_COEFFICIENT_GROUPS(X) \
    X(Simulation, SimulationModelCoefficients, SHR_SIMULATION_MODEL_COEFFICIENTS) \
    X(Rhythm, RhythmModelCoefficients, SHR_RHYTHM_MODEL_COEFFICIENTS)

#define SHR_NONLIVE_MODEL_CONSTANTS(X) \
    X(Dormant, float, Dormant, "is intentionally dormant")
