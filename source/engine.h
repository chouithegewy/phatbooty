// Phat Booty Bass: groove generator + mono bass synth.
// Plain C++ with no VST dependencies, so it can be driven by the plug-in or the offline renderer.
#pragma once

#include "params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#define PHATBOOTY_HAS_SSE 1
#endif

namespace PhatBooty {

constexpr int kStepsPerBar = 16;
constexpr int kMaxSteps = kStepsPerBar * 4;
constexpr double kPi = 3.14159265358979323846;

//------------------------------------------------------------------------
// Groove generator
//------------------------------------------------------------------------
struct Step
{
	bool on = false;
	bool ghost = false;
	bool accent = false;
	bool slide = false; // hold the gate and glide into the next step
	int semis = 0;      // offset from the held root note
	float gate = 0.5f;  // fraction of the step length
};

class Pattern
{
public:
	Step steps[kMaxSteps];
	int length = kStepsPerBar * 2;

	// Every step gets a fixed set of dice rolls derived from (seed, step), then the knobs only move
	// thresholds. Turning Density or Funk therefore adds/removes notes without reshuffling the groove.
	void generate (const double* plain)
	{
		const int seed = static_cast<int> (plain[kSeed]);
		const double density = plain[kDensity] / 100.;
		const double funk = plain[kFunk] / 100.;
		const double octPops = plain[kOctavePops] / 100.;
		const double slides = plain[kSlides] / 100.;
		const double ghosts = plain[kGhosts] / 100.;
		const double gate = plain[kGate] / 100.;
		const int scale = std::clamp (static_cast<int> (plain[kScale]), 0, kNumScales - 1);
		const int bars = 1 << std::clamp (static_cast<int> (plain[kLength]), 0, kNumLengths - 1);

		length = bars * kStepsPerBar;

		// How likely each 16th of the bar is to be played: straight rock feel vs. syncopated funk.
		static constexpr float straightW[kStepsPerBar] = {1.f, .1f, .45f, .15f, .75f, .1f, .45f, .15f,
		                                                  .8f, .1f, .45f, .15f, .75f, .1f, .45f, .2f};
		static constexpr float funkW[kStepsPerBar] = {1.f, .35f, .45f, .85f, .3f, .55f, .75f, .5f,
		                                              .5f, .4f, .8f, .85f, .3f, .6f, .75f, .6f};

		Dice dice[kMaxSteps];
		for (int i = 0; i < length; ++i)
		{
			const int pos = i % kStepsPerBar;
			const int bar = i / kStepsPerBar;
			const bool turnaround = (bar == bars - 1) && pos >= 12;

			// Later bars mostly repeat bar one (call and response), with a fresh fill at the end.
			int diceIndex = i;
			if (bar > 0 && !turnaround && roll (seed, 1000 + i).repeat < 0.7f)
				diceIndex = pos;
			const Dice d = roll (seed, diceIndex);
			dice[i] = d;

			Step& st = steps[i];
			st = Step {};

			const float w = straightW[pos] + (funkW[pos] - straightW[pos]) * static_cast<float> (funk);
			st.on = pos == 0 || d.on < density * w * 1.35;

			if (!st.on)
			{
				if (d.ghost < ghosts * 0.55)
				{
					st.on = st.ghost = true;
					st.semis = d.oct < 0.5f ? 0 : 12;
					st.gate = 0.18f;
				}
				continue;
			}

			st.semis = pos == 0 ? 0 : pickInterval (scale, d.note, pos % 4 == 0);

			if (pos == kStepsPerBar - 1 && d.approach < 0.3 + 0.35 * funk)
				st.semis = -1; // chromatic approach into the next One

			if (pos != 0 && d.oct < octPops * (pos % 2 ? 0.8 : 0.45))
				st.semis += 12;

			st.accent = pos == 0 || (pos % 4 == 0 ? d.accent < 0.45f : d.accent < 0.1 + 0.3 * funk);
			st.gate = static_cast<float> (std::clamp (gate * (0.75 + 0.5 * d.gate), 0.05, 1.0));
		}

		for (int i = 0; i < length; ++i)
		{
			const Step& next = steps[(i + 1) % length];
			Step& st = steps[i];
			st.slide = st.on && !st.ghost && next.on && !next.ghost && dice[i].slide < slides * 0.6;
		}
	}

private:
	struct Dice
	{
		float on, note, oct, slide, ghost, accent, gate, approach, repeat;
	};

	static uint64_t splitmix (uint64_t& s)
	{
		uint64_t z = (s += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	static Dice roll (int seed, int index)
	{
		uint64_t s = (static_cast<uint64_t> (seed) << 24) ^ (static_cast<uint64_t> (index) * 0x51ED27ull);
		auto u = [&] () { return static_cast<float> (splitmix (s) >> 40) / static_cast<float> (1 << 24); };
		Dice d;
		d.on = u ();
		d.note = u ();
		d.oct = u ();
		d.slide = u ();
		d.ghost = u ();
		d.accent = u ();
		d.gate = u ();
		d.approach = u ();
		d.repeat = u ();
		return d;
	}

	// Weighted pick of a scale degree; the root, fifth and flat seven do most of the work.
	static int pickInterval (int scale, float r, bool downbeat)
	{
		struct Scale
		{
			int count;
			int semis[7];
			float weight[7];
		};
		static constexpr Scale scales[kNumScales] = {
		    {5, {0, 3, 5, 7, 10}, {5.f, 1.2f, 1.f, 2.5f, 1.8f}},                         // minor pent
		    {6, {0, 3, 5, 6, 7, 10}, {5.f, 1.2f, 1.f, .7f, 2.5f, 1.8f}},                 // blues
		    {7, {0, 2, 3, 5, 7, 9, 10}, {5.f, .5f, 1.2f, 1.f, 2.5f, .7f, 1.6f}},         // dorian
		    {7, {0, 2, 3, 5, 7, 8, 10}, {5.f, .5f, 1.2f, 1.f, 2.5f, .6f, 1.6f}},         // minor
		    {7, {0, 2, 4, 5, 7, 9, 10}, {5.f, .5f, 1.2f, 1.f, 2.5f, .7f, 1.6f}},         // mixolydian
		    {7, {0, 1, 3, 5, 7, 8, 10}, {5.f, 1.f, 1.2f, 1.f, 2.5f, .6f, 1.2f}},         // phrygian
		};
		const Scale& sc = scales[scale];
		float total = 0.f;
		for (int i = 0; i < sc.count; ++i)
			total += sc.weight[i] * (i == 0 && downbeat ? 2.f : 1.f);
		float x = r * total;
		for (int i = 0; i < sc.count; ++i)
		{
			x -= sc.weight[i] * (i == 0 && downbeat ? 2.f : 1.f);
			if (x <= 0.f)
				return sc.semis[i];
		}
		return 0;
	}
};

//------------------------------------------------------------------------
// Mono bass voice: PolyBLEP saw/square + sine sub, 24 dB resonant low-pass, drive
//------------------------------------------------------------------------
class Voice
{
public:
	void setSampleRate (double rate)
	{
		sr = rate;
		reset ();
	}

	void reset ()
	{
		phase = subPhase = 0.;
		ampEnv = filtEnv = 0.f;
		gate = false;
		s1a = s2a = s1b = s2b = 0.f;
		hpIn = hpOut = 0.f;
	}

	// legato: keep the envelopes running and glide to the new pitch (a 303-style slide)
	void trigger (int note, float velocity, bool isAccent, bool legato, double glideMs)
	{
		targetPitch = static_cast<float> (note);
		if (legato && glideMs > 0.)
		{
			glideCoef = static_cast<float> (1. - std::exp (-1. / (glideMs * 0.001 * sr)));
		}
		else
		{
			pitch = targetPitch;
			glideCoef = 1.f;
		}
		if (!legato || !gate)
		{
			filtEnv = 1.f;
			vel = velocity;
			accent = isAccent;
		}
		gate = true;
	}

	void release () { gate = false; }
	bool isGateOpen () const { return gate; }

	struct Settings
	{
		float wave, sub, cutoff, resonance, envAmount, decayMs, accentAmt, drive;
	};

	float process (const Settings& s)
	{
		pitch += (targetPitch - pitch) * glideCoef;

		// Envelopes: fast attack, full sustain while the gate is open, short release.
		if (gate)
			ampEnv += (1.f - ampEnv) * attackCoef ();
		else
			ampEnv *= releaseCoef ();
		if (ampEnv < 1e-6f)
			ampEnv = 0.f;

		const float acc = accent ? s.accentAmt : 0.f;
		filtEnv *= decayCoef (s.decayMs * (1.f - 0.4f * acc));
		if (filtEnv < 1e-6f)
			filtEnv = 0.f;

		// Oscillators
		const double freq = 440. * std::exp2 ((pitch - 69.f) / 12.);
		const double dt = std::min (freq / sr, 0.45);
		const float saw = static_cast<float> (2. * phase - 1. - blep (phase, dt));
		double ph2 = phase + 0.5;
		if (ph2 >= 1.)
			ph2 -= 1.;
		const float square = static_cast<float> ((phase < 0.5 ? 1. : -1.) + blep (phase, dt) - blep (ph2, dt));
		const float sub = static_cast<float> (std::sin (2. * kPi * subPhase));
		phase += dt;
		if (phase >= 1.)
			phase -= 1.;
		subPhase += dt * 0.5;
		if (subPhase >= 1.)
			subPhase -= 1.;

		// Filter: cutoff from knob, key tracking and the (accent-boosted) envelope
		const float octaves = s.envAmount * 6.f * filtEnv * (1.f + 0.6f * acc) + (pitch - 36.f) / 24.f;
		double fc = 30. * std::exp2 (s.cutoff * 9. + octaves);
		fc = std::clamp (fc, 20., sr * 0.45);
		const float g = static_cast<float> (std::tan (kPi * fc / sr));
		const float k = 2.f - 1.96f * s.resonance;

		const float osc = (1.f - s.wave) * saw + s.wave * square * 0.8f;
		float y = svf (std::tanh (osc * 1.2f), g, k, s1a, s2a);
		y = svf (y, g, 1.414f, s1b, s2b);
		y *= 1.f + 0.6f * s.resonance; // keep the low end up when resonance thins it out

		y += sub * s.sub * 0.9f;

		const float level = ampEnv * (0.55f + 0.45f * vel) * (1.f + 0.35f * acc);
		const float driveGain = 1.f + s.drive * 7.f;
		y = std::tanh (y * level * driveGain) / (1.f + s.drive * 1.6f);

		// DC / subsonic blocker
		const float hp = y - hpIn + 0.9985f * hpOut;
		hpIn = y;
		hpOut = hp;
		return hp;
	}

private:
	static double blep (double t, double dt)
	{
		if (t < dt)
		{
			t /= dt;
			return t + t - t * t - 1.;
		}
		if (t > 1. - dt)
		{
			t = (t - 1.) / dt;
			return t * t + t + t + 1.;
		}
		return 0.;
	}

	// Andy Simper's trapezoidal SVF, low-pass output
	static float svf (float v0, float g, float k, float& ic1, float& ic2)
	{
		const float a1 = 1.f / (1.f + g * (g + k));
		const float a2 = g * a1;
		const float a3 = g * a2;
		const float v3 = v0 - ic2;
		const float v1 = a1 * ic1 + a2 * v3;
		const float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		return v2;
	}

	float attackCoef () const { return static_cast<float> (1. - std::exp (-1. / (0.0015 * sr))); }
	float releaseCoef () const { return static_cast<float> (std::exp (-1. / (0.012 * sr))); }
	float decayCoef (float ms)
	{
		if (ms != cachedDecayMs)
		{
			cachedDecayMs = ms;
			cachedDecayCoef = static_cast<float> (std::exp (-1. / (std::max (ms, 1.f) * 0.001 * sr)));
		}
		return cachedDecayCoef;
	}

	double sr = 44100.;
	double phase = 0., subPhase = 0.;
	float pitch = 36.f, targetPitch = 36.f, glideCoef = 1.f;
	float ampEnv = 0.f, filtEnv = 0.f, vel = 1.f;
	bool gate = false, accent = false;
	float s1a = 0.f, s2a = 0.f, s1b = 0.f, s2b = 0.f;
	float hpIn = 0.f, hpOut = 0.f;
	float cachedDecayMs = -1.f, cachedDecayCoef = 0.f;
};

//------------------------------------------------------------------------
// Engine: held keys + host-synced step sequencer + voice
//------------------------------------------------------------------------
class Engine
{
public:
	struct Transport
	{
		bool playing = false;
		double ppq = 0.;      // musical position at the start of the block, in quarter notes
		double tempo = 120.;  // BPM
	};

	// MIDI mirror of what the voice plays, collected per block for the plug-in's MIDI output.
	struct MidiEvent
	{
		int sampleOffset;
		int pitch;
		float velocity;
		bool on;
	};
	static constexpr int kMaxMidiEvents = 256;

	Engine ()
	{
		for (int i = 0; i < kNumParams; ++i)
			plain[i] = kParams[i].defaultPlain;
		pattern.generate (plain);
		snapSmoothers ();
	}

	void setSampleRate (double rate)
	{
		sr = rate;
		voice.setSampleRate (rate);
		smoothCoef = static_cast<float> (1. - std::exp (-1. / (0.005 * sr)));
	}

	void reset ()
	{
		voice.reset ();
		soundingNote = -1;
		numMidiEvents = 0;
		numHeld = 0;
		seqRunning = false;
		gateOpen = false;
		snapSmoothers ();
	}

	void setParamNormalized (int id, double value)
	{
		if (id < 0 || id >= kNumParams)
			return;
		const double v = toPlain (id, std::clamp (value, 0., 1.));
		if (v == plain[id])
			return;
		plain[id] = v;
		if (id <= kLength && id != kGrooveOn)
			patternDirty = true;
		if (id == kGrooveOn)
			grooveModeChanged ();
	}

	double getParamNormalized (int id) const { return toNormalized (id, plain[id]); }
	const Pattern& getPattern () const { return pattern; }

	// Pattern step currently playing, or -1 when the groove isn't running.
	int currentStep () const
	{
		if (!seqRunning || lastStep == INT64_MIN)
			return -1;
		const int len = pattern.length;
		return static_cast<int> (((lastStep % len) + len) % len);
	}

	// Last key the groove was transposed to (for exporting riffs in the right key).
	int rootNote () const { return numHeld > 0 ? held[numHeld - 1].pitch : lastRoot; }

	int numMidiOut () const { return numMidiEvents; }
	const MidiEvent& midiOut (int i) const { return midiEvents[i]; }

	// Call once per block before render(); sample offsets in noteOn/noteOff/render are block-relative.
	void beginBlock (const Transport& t)
	{
		if (patternDirty)
		{
			pattern.generate (plain);
			patternDirty = false;
		}
		const bool wasPlaying = transport.playing;
		transport = t;
		if (transport.tempo <= 0.)
			transport.tempo = 120.;
		if (wasPlaying && !transport.playing)
			internalPpq = lastPpq; // keep grooving from where the host stopped
		blockPos = 0;
		numMidiEvents = 0;
	}

	void noteOn (int pitch, float velocity)
	{
		removeHeld (pitch);
		if (numHeld == kMaxHeld)
			removeHeld (held[0].pitch);
		const bool wasEmpty = numHeld == 0;
		held[numHeld++] = {pitch, velocity};
		lastRoot = pitch;
		eventSample = blockPos;

		if (grooveOn ())
		{
			if (wasEmpty)
				startSequencer ();
		}
		else
		{
			playNote (pitch, velocity, velocity > 0.9f, !wasEmpty);
		}
	}

	void noteOff (int pitch)
	{
		const bool wasTop = numHeld > 0 && held[numHeld - 1].pitch == pitch;
		removeHeld (pitch);
		eventSample = blockPos;
		if (numHeld == 0)
		{
			seqRunning = false;
			gateOpen = false;
			releaseNote ();
		}
		else if (!grooveOn () && wasTop)
		{
			const auto& h = held[numHeld - 1];
			playNote (h.pitch, h.velocity, false, true);
		}
	}

	void allNotesOff ()
	{
		numHeld = 0;
		seqRunning = false;
		gateOpen = false;
		eventSample = blockPos;
		releaseNote ();
	}

	// Renders samples [start, end) of the current block. Output is mono duplicated to both channels.
	void render (float* left, float* right, int start, int end)
	{
#if PHATBOOTY_HAS_SSE
		const unsigned int csr = _mm_getcsr ();
		_mm_setcsr (csr | 0x8040); // flush denormals to zero
#endif
		const double ppqPerSample = transport.tempo / 60. / sr;
		const float volume = static_cast<float> (plain[kVolume] / 100.);
		const float keyVel = numHeld > 0 ? 0.6f + 0.4f * held[numHeld - 1].velocity : lastKeyVel;
		if (numHeld > 0)
			lastKeyVel = keyVel;

		for (int i = start; i < end; ++i)
		{
			if (seqRunning)
			{
				double ppq;
				if (transport.playing)
					ppq = transport.ppq + (blockPos + (i - start)) * ppqPerSample;
				else
				{
					ppq = internalPpq;
					internalPpq += ppqPerSample;
				}
				lastPpq = ppq;
				eventSample = i;
				tick (ppq);
			}
			else if (transport.playing)
			{
				lastPpq = transport.ppq + (blockPos + (i - start)) * ppqPerSample;
			}

			smooth (sWave, plain[kWave] / 100.);
			smooth (sSub, plain[kSub] / 100.);
			smooth (sCutoff, plain[kCutoff] / 100.);
			smooth (sRes, plain[kResonance] / 100.);
			smooth (sDrive, plain[kDrive] / 100.);
			smooth (sVolume, 1.5 * volume * volume);

			const Voice::Settings vs {sWave,
			                          sSub,
			                          sCutoff,
			                          sRes,
			                          static_cast<float> (plain[kEnvAmount] / 100.),
			                          static_cast<float> (plain[kDecay]),
			                          static_cast<float> (plain[kAccent] / 100.),
			                          sDrive};
			const float out = voice.process (vs) * sVolume * keyVel;
			left[i] = out;
			if (right)
				right[i] = out;
		}
		blockPos += end - start;
#if PHATBOOTY_HAS_SSE
		_mm_setcsr (csr);
#endif
	}

private:
	struct Held
	{
		int pitch;
		float velocity;
	};
	static constexpr int kMaxHeld = 16;

	bool grooveOn () const { return plain[kGrooveOn] > 0.5; }
	double swing () const { return plain[kSwing] / 100.; }

	// Plays a note on the voice and mirrors it to MIDI out. Legato notes overlap the previous one
	// (new note-on before the old note-off) so a receiving mono synth slides too.
	void playNote (int note, float velocity, bool isAccent, bool legato)
	{
		voice.trigger (note, velocity, isAccent, legato, plain[kGlide]);
		const int prev = soundingNote;
		if (prev >= 0 && (!legato || prev == note))
			pushMidi (prev, 0.f, false);
		pushMidi (note, velocity, true);
		if (prev >= 0 && legato && prev != note)
			pushMidi (prev, 0.f, false);
		soundingNote = note;
	}

	void releaseNote ()
	{
		voice.release ();
		if (soundingNote >= 0)
			pushMidi (soundingNote, 0.f, false);
		soundingNote = -1;
	}

	void pushMidi (int pitch, float velocity, bool on)
	{
		if (numMidiEvents < kMaxMidiEvents)
			midiEvents[numMidiEvents++] = {eventSample, pitch, velocity, on};
	}

	void grooveModeChanged ()
	{
		seqRunning = false;
		gateOpen = false;
		eventSample = blockPos;
		releaseNote ();
		if (numHeld > 0 && grooveOn ())
			startSequencer ();
	}

	void startSequencer ()
	{
		seqRunning = true;
		gateOpen = false;
		prevSlide = false;
		lastStep = INT64_MIN; // fire whatever step we land on
		if (!transport.playing)
			internalPpq = 0.;
	}

	// 16th-note grid; every second 16th is pushed late by the swing amount.
	int64_t stepIndexAt (double ppq) const
	{
		const double pair = std::floor (ppq / 0.5);
		const double within = ppq - pair * 0.5;
		return static_cast<int64_t> (pair) * 2 + (within >= swing () * 0.5 ? 1 : 0);
	}

	double stepStart (int64_t idx) const
	{
		const int64_t pair = idx >= 0 ? idx / 2 : -((-idx + 1) / 2);
		const bool odd = (idx - pair * 2) != 0;
		return static_cast<double> (pair) * 0.5 + (odd ? swing () * 0.5 : 0.);
	}

	void tick (double ppq)
	{
		const int64_t idx = stepIndexAt (ppq);
		if (idx != lastStep)
		{
			lastStep = idx;
			triggerStep (idx);
		}
		if (gateOpen && !holdGate && ppq >= gateOffPpq)
		{
			releaseNote ();
			gateOpen = false;
		}
	}

	void triggerStep (int64_t idx)
	{
		const int len = pattern.length;
		const int pos = static_cast<int> (((idx % len) + len) % len);
		const Step& st = pattern.steps[pos];
		if (!st.on || numHeld == 0)
		{
			if (gateOpen)
				releaseNote ();
			gateOpen = holdGate = prevSlide = false;
			return;
		}

		const int note = std::clamp (held[numHeld - 1].pitch + st.semis, 0, 127);
		const float vel = st.ghost ? 0.3f : (st.accent ? 1.f : 0.7f);
		const bool legato = prevSlide && gateOpen;
		playNote (note, vel, st.accent, legato);

		const double start = stepStart (idx);
		const double end = stepStart (idx + 1);
		gateOpen = true;
		holdGate = st.slide;
		gateOffPpq = start + (end - start) * st.gate;
		prevSlide = st.slide;
	}

	void removeHeld (int pitch)
	{
		int j = 0;
		for (int i = 0; i < numHeld; ++i)
			if (held[i].pitch != pitch)
				held[j++] = held[i];
		numHeld = j;
	}

	void smooth (float& state, double target)
	{
		state += (static_cast<float> (target) - state) * smoothCoef;
	}

	void snapSmoothers ()
	{
		sWave = static_cast<float> (plain[kWave] / 100.);
		sSub = static_cast<float> (plain[kSub] / 100.);
		sCutoff = static_cast<float> (plain[kCutoff] / 100.);
		sRes = static_cast<float> (plain[kResonance] / 100.);
		sDrive = static_cast<float> (plain[kDrive] / 100.);
		const double v = plain[kVolume] / 100.;
		sVolume = static_cast<float> (1.5 * v * v);
	}

	double plain[kNumParams];
	bool patternDirty = false;
	Pattern pattern;
	Voice voice;

	double sr = 44100.;
	float smoothCoef = 0.005f;
	float sWave = 0, sSub = 0, sCutoff = 0, sRes = 0, sDrive = 0, sVolume = 0;

	Held held[kMaxHeld];
	int numHeld = 0;
	int lastRoot = 33;

	MidiEvent midiEvents[kMaxMidiEvents];
	int numMidiEvents = 0;
	int eventSample = 0;   // block-relative sample position for emitted MIDI
	int soundingNote = -1; // note currently held on MIDI out
	float lastKeyVel = 1.f;

	Transport transport;
	int blockPos = 0;
	double internalPpq = 0., lastPpq = 0.;
	bool seqRunning = false;
	int64_t lastStep = INT64_MIN;
	bool gateOpen = false, holdGate = false, prevSlide = false;
	double gateOffPpq = 0.;
};

} // namespace PhatBooty
