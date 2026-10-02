// Parameter definitions shared by the processor, the controller and the offline renderer.
#pragma once

namespace PhatBooty {

enum ParamIds : int
{
	kGrooveOn = 0,
	kSeed,
	kDensity,
	kFunk,
	kSwing,
	kOctavePops,
	kSlides,
	kGhosts,
	kGate,
	kScale,
	kLength,
	kWave,
	kSub,
	kCutoff,
	kResonance,
	kEnvAmount,
	kDecay,
	kAccent,
	kDrive,
	kGlide,
	kVolume,

	kNumParams,

	// Read-only output (not saved): current pattern step + 1, scaled by 1/kPlayheadSteps; 0 = idle.
	kPlayhead = 100,
	// Read-only output (not saved): last key the groove was played in, as pitch / 127.
	kRootNote = 101
};

inline constexpr int kPlayheadSteps = 64;

struct ParamInfo
{
	const char* name;
	const char* units;
	double minPlain;
	double maxPlain;
	double defaultPlain;
	int stepCount;          // 0 = continuous, 1 = toggle, N = N+1 discrete values
	const char* const* list; // non-null for list parameters
};

inline constexpr const char* kScaleNames[] = {"Minor Pentatonic", "Blues", "Dorian", "Minor",
                                              "Mixolydian", "Phrygian"};
inline constexpr int kNumScales = 6;

inline constexpr const char* kLengthNames[] = {"1 Bar", "2 Bars", "4 Bars"};
inline constexpr int kNumLengths = 3;

// Every parameter is exposed to the host as 0..1 and mapped linearly onto [minPlain, maxPlain].
inline constexpr ParamInfo kParams[kNumParams] = {
    {"Groove", "", 0, 1, 1, 1, nullptr},
    {"Seed", "", 0, 999, 42, 999, nullptr},
    {"Density", "%", 0, 100, 60, 0, nullptr},
    {"Funk", "%", 0, 100, 60, 0, nullptr},
    {"Swing", "%", 50, 75, 56, 0, nullptr},
    {"Octave Pops", "%", 0, 100, 35, 0, nullptr},
    {"Slides", "%", 0, 100, 25, 0, nullptr},
    {"Ghost Notes", "%", 0, 100, 30, 0, nullptr},
    {"Gate", "%", 10, 100, 55, 0, nullptr},
    {"Scale", "", 0, kNumScales - 1, 0, kNumScales - 1, kScaleNames},
    {"Length", "", 0, kNumLengths - 1, 1, kNumLengths - 1, kLengthNames},
    {"Saw/Square", "%", 0, 100, 30, 0, nullptr},
    {"Sub", "%", 0, 100, 60, 0, nullptr},
    {"Cutoff", "%", 0, 100, 30, 0, nullptr},
    {"Resonance", "%", 0, 100, 45, 0, nullptr},
    {"Env Amount", "%", 0, 100, 55, 0, nullptr},
    {"Decay", "ms", 20, 1500, 220, 0, nullptr},
    {"Accent", "%", 0, 100, 60, 0, nullptr},
    {"Drive", "%", 0, 100, 40, 0, nullptr},
    {"Glide", "ms", 0, 400, 60, 0, nullptr},
    {"Volume", "%", 0, 100, 70, 0, nullptr},
};

inline double toNormalized (int id, double plain)
{
	const auto& p = kParams[id];
	return (plain - p.minPlain) / (p.maxPlain - p.minPlain);
}

inline double toPlain (int id, double normalized)
{
	const auto& p = kParams[id];
	double v = p.minPlain + normalized * (p.maxPlain - p.minPlain);
	if (p.stepCount > 0)
		v = static_cast<double> (static_cast<long long> (v + 0.5));
	return v;
}

} // namespace PhatBooty
