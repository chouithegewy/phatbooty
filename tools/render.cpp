// Offline renderer: holds a key, runs the groove engine and writes a 16-bit stereo WAV.
// Usage: phatbooty_render out.wav [bars=4] [bpm=100] [note=33] [Param=value ...]
//   Param names are the parameter names without spaces (e.g. Seed=7 Funk=80 Cutoff=40).
//   mid=riff.mid also writes the pattern as a MIDI file.
#include "engine.h"
#include "midifile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace PhatBooty;

static void writeWav (const char* path, const std::vector<float>& mono, int sr)
{
	FILE* f = std::fopen (path, "wb");
	if (!f)
	{
		std::perror (path);
		std::exit (1);
	}
	auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
	auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
	const uint32_t dataBytes = static_cast<uint32_t> (mono.size () * 4);
	std::fwrite ("RIFF", 1, 4, f);
	u32 (36 + dataBytes);
	std::fwrite ("WAVEfmt ", 1, 8, f);
	u32 (16);
	u16 (1);
	u16 (2);
	u32 (sr);
	u32 (sr * 4);
	u16 (4);
	u16 (16);
	std::fwrite ("data", 1, 4, f);
	u32 (dataBytes);
	for (float x : mono)
	{
		const auto s = static_cast<int16_t> (std::clamp (x, -1.f, 1.f) * 32767.f);
		u16 (s);
		u16 (s);
	}
	std::fclose (f);
}

static std::string squash (const char* s)
{
	std::string r;
	for (; *s; ++s)
		if (*s != ' ' && *s != '/')
			r += *s;
	return r;
}

int main (int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf (stderr, "usage: %s out.wav [bars] [bpm] [note] [Param=value ...]\n", argv[0]);
		return 1;
	}
	int bars = 4;
	double bpm = 100.;
	int note = 33; // MIDI 33: low A
	int argi = 2;
	auto isNum = [] (const char* s) { return std::strchr (s, '=') == nullptr; };
	if (argi < argc && isNum (argv[argi]))
		bars = std::atoi (argv[argi++]);
	if (argi < argc && isNum (argv[argi]))
		bpm = std::atof (argv[argi++]);
	if (argi < argc && isNum (argv[argi]))
		note = std::atoi (argv[argi++]);

	const int sr = 48000;
	Engine engine;
	engine.setSampleRate (sr);
	engine.reset ();

	std::string midPath;
	for (; argi < argc; ++argi)
	{
		const char* eq = std::strchr (argv[argi], '=');
		const std::string key (argv[argi], static_cast<size_t> (eq - argv[argi]));
		if (key == "mid")
		{
			midPath = eq + 1;
			continue;
		}
		bool found = false;
		for (int id = 0; id < kNumParams; ++id)
		{
			if (squash (kParams[id].name) == key)
			{
				engine.setParamNormalized (id, toNormalized (id, std::atof (eq + 1)));
				found = true;
			}
		}
		if (!found)
		{
			std::fprintf (stderr, "unknown parameter '%s'\n", key.c_str ());
			return 1;
		}
	}

	const int block = 256;
	const int total = static_cast<int> (bars * 4 * 60. / bpm * sr) + sr / 2;
	std::vector<float> out (total), scratch (block);
	double ppq = 0.;
	const int noteOffAt = total - sr / 2;
	for (int pos = 0; pos < total; pos += block)
	{
		const int n = std::min (block, total - pos);
		engine.beginBlock ({true, ppq, bpm});
		int start = 0;
		if (pos == 0)
			engine.noteOn (note, 0.9f);
		if (noteOffAt >= pos && noteOffAt < pos + n)
		{
			engine.render (out.data () + pos, nullptr, 0, noteOffAt - pos);
			engine.noteOff (note);
			start = noteOffAt - pos;
		}
		// render() indexes the buffer by block offset, so pass the block base pointer.
		engine.render (out.data () + pos, nullptr, start, n);
		ppq += n * bpm / 60. / sr;
	}

	// Print the first bars of the pattern as a step grid.
	const Pattern& p = engine.getPattern ();
	static const char* names[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
	for (int i = 0; i < p.length; ++i)
	{
		if (i % kStepsPerBar == 0)
			std::printf ("bar %d: ", i / kStepsPerBar + 1);
		const Step& s = p.steps[i];
		if (!s.on)
			std::printf (" .   ");
		else
		{
			const int n = note + s.semis;
			std::printf ("%s%-2s%c%c ", s.ghost ? "(" : (s.accent ? "!" : " "), names[((n % 12) + 12) % 12],
			             s.semis >= 12 ? '\'' : ' ', s.slide ? '~' : (s.ghost ? ')' : ' '));
		}
		if (i % kStepsPerBar == kStepsPerBar - 1)
			std::printf ("\n");
	}

	float peak = 0.f;
	double rms = 0.;
	for (float x : out)
	{
		peak = std::max (peak, std::fabs (x));
		rms += x * x;
	}
	rms = std::sqrt (rms / out.size ());
	std::printf ("peak %.3f (%.1f dBFS), rms %.1f dBFS, %zu samples\n", peak, 20 * std::log10 (peak + 1e-9),
	             20 * std::log10 (rms + 1e-9), out.size ());
	writeWav (argv[1], out, sr);

	if (!midPath.empty ())
	{
		const auto bytes = buildMidiFile (engine.getPattern (), note,
		                                  engine.getParamNormalized (kSwing) * 0.25 + 0.5, "PhatBooty");
		FILE* f = std::fopen (midPath.c_str (), "wb");
		if (!f)
		{
			std::perror (midPath.c_str ());
			return 1;
		}
		std::fwrite (bytes.data (), 1, bytes.size (), f);
		std::fclose (f);
	}
	return 0;
}
