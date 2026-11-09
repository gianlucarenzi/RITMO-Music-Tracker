#pragma once

// The twelve semitones of the 2:1 octave (RMT 1.36, plan 59): every
// selectable temperament has this many notes.
constexpr int NOTES_PER_OCTAVE = 12;

// The musical cents of the 2:1 octave (one equal-tempered semitone is
// CENTS_PER_OCTAVE / NOTES_PER_OCTAVE = 100): a multiplier m is
// CENTS_PER_OCTAVE * log2(m) cents.
constexpr int CENTS_PER_OCTAVE = 1200;

// The twelve tonic-based values of the custom temperament, in musical cents
// (1 cent = 1/1200 octave, 100 cents = one equal-tempered semitone).
// values[0] is the tonic and always 0; the octave is fixed at 1200 (2:1) and
// not stored. They replace the 13 numerator/denominator fractions of
// TTuningRatios (RMT 1.36, plan 59): cents are the notation of the tuning
// literature, and not every temperament can be written as a fraction. A
// multiplier m converts as cents = 1200*log2(m); the table generation
// applies 2^(cents/1200).
struct TTuningCents {
    static const int COUNT = NOTES_PER_OCTAVE; // one cents value per semitone

    // cents above the tonic, by semitone; values[0] stays 0
    double values[COUNT];

    void Initialize();
};

struct TTuningSettings {
    // The default A4 reference pitch: RMT's traditional tuning rather than
    // concert pitch 440 Hz, the value the Patch16 player's tables were
    // generated with.
    static constexpr double PAL_A4_FREQUENCY = 440.83751645933;

    // The PAL default scaled by exactly the machine-clock ratio, the
    // conversion of the PAL/NTSC switch, so both defaults generate the same
    // tables (RMT 1.36, e072b3a; the former literal 444.895778867913 was a
    // truncated decimal of that value). Defined in TuningTypes.cpp, where the
    // clock constants of CAtari are visible.
    static const double NTSC_A4_FREQUENCY;

    double a4Frequency; // the pitch of A4 in Hz, in every key (RMT 1.36, plan 59 step 2)
    int tonic;          // the key the tables are tuned to: 3 = A-, counted downward from C- = 0
    int temperament = 0; // 0 = equal temperament, 1..28 the presets, CTuning::TUNING_CUSTOM the custom one
    TTuningCents cents;  // the cents the custom temperament is played from

    void Initialize(bool ntsc);
};
