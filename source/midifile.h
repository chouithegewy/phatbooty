// Writes a groove pattern as a Standard MIDI File (format 0, one pass through the pattern).
#pragma once

#include "engine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PhatBooty {

inline std::vector<uint8_t> buildMidiFile (const Pattern& pattern, int root, double swing, const std::string& name)
{
	constexpr int kPPQ = 480;
	struct Ev
	{
		uint32_t tick;
		int order; // note-offs sort before note-ons on the same tick
		uint8_t status, data1, data2;
	};

	// Same swing grid as the engine: every second 16th is pushed late.
	auto stepTick = [&] (int idx) {
		const uint32_t pair = static_cast<uint32_t> (idx / 2);
		return pair * (kPPQ / 2) + (idx % 2 ? static_cast<uint32_t> (swing * kPPQ / 2. + 0.5) : 0u);
	};
	const uint32_t total = static_cast<uint32_t> (pattern.length) * (kPPQ / 4);

	std::vector<Ev> events;
	for (int i = 0; i < pattern.length; ++i)
	{
		const Step& st = pattern.steps[i];
		if (!st.on)
			continue;
		const auto note = static_cast<uint8_t> (std::clamp (root + st.semis, 0, 127));
		const uint8_t vel = st.ghost ? 38 : (st.accent ? 127 : 89);
		const uint32_t start = stepTick (i);
		const uint32_t next = stepTick (i + 1);
		uint32_t end = start + std::max<uint32_t> (10, static_cast<uint32_t> ((next - start) * st.gate));
		if (st.slide && i + 1 < pattern.length)
		{
			// Overlap into the next note so mono synths glide; same pitch can't overlap itself.
			const int nextNote = std::clamp (root + pattern.steps[i + 1].semis, 0, 127);
			end = nextNote == note ? next : next + 30;
		}
		end = std::min (end, total);
		events.push_back ({start, 1, 0x90, note, vel});
		events.push_back ({end, 0, 0x80, note, 0});
	}
	std::stable_sort (events.begin (), events.end (), [] (const Ev& a, const Ev& b) {
		return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
	});

	std::vector<uint8_t> track;
	auto varLen = [&] (uint32_t v) {
		uint8_t buf[4];
		int n = 0;
		buf[n++] = v & 0x7F;
		while (v >>= 7)
			buf[n++] = static_cast<uint8_t> ((v & 0x7F) | 0x80);
		while (n--)
			track.push_back (buf[n]);
	};
	auto meta = [&] (uint8_t type, const std::vector<uint8_t>& payload) {
		varLen (0);
		track.push_back (0xFF);
		track.push_back (type);
		varLen (static_cast<uint32_t> (payload.size ()));
		track.insert (track.end (), payload.begin (), payload.end ());
	};

	meta (0x03, std::vector<uint8_t> (name.begin (), name.end ())); // track name
	meta (0x58, {4, 2, 24, 8});                                      // 4/4
	uint32_t now = 0;
	for (const Ev& e : events)
	{
		varLen (e.tick - now);
		now = e.tick;
		track.push_back (e.status);
		track.push_back (e.data1);
		track.push_back (e.data2);
	}
	varLen (total - now); // end the clip on the bar line
	track.insert (track.end (), {0xFF, 0x2F, 0x00});

	std::vector<uint8_t> file = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, kPPQ >> 8, kPPQ & 0xFF,
	                             'M', 'T', 'r', 'k'};
	const auto len = static_cast<uint32_t> (track.size ());
	file.insert (file.end (), {static_cast<uint8_t> (len >> 24), static_cast<uint8_t> (len >> 16),
	                           static_cast<uint8_t> (len >> 8), static_cast<uint8_t> (len)});
	file.insert (file.end (), track.begin (), track.end ());
	return file;
}

} // namespace PhatBooty
