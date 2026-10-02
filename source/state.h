// Shared preset/state (de)serialization: a version tag followed by every normalized parameter.
#pragma once

#include "params.h"

#include "base/source/fstreamer.h"

namespace PhatBooty {

constexpr Steinberg::int32 kStateVersion = 1;

inline bool writeState (Steinberg::IBStreamer& s, const double* normalized)
{
	if (!s.writeInt32 (kStateVersion))
		return false;
	for (int i = 0; i < kNumParams; ++i)
		if (!s.writeDouble (normalized[i]))
			return false;
	return true;
}

// Missing trailing values (older states) keep their defaults.
inline bool readState (Steinberg::IBStreamer& s, double* normalized)
{
	Steinberg::int32 version = 0;
	if (!s.readInt32 (version) || version < 1)
		return false;
	for (int i = 0; i < kNumParams; ++i)
	{
		double v;
		if (!s.readDouble (v))
			break;
		normalized[i] = v < 0. ? 0. : (v > 1. ? 1. : v);
	}
	return true;
}

} // namespace PhatBooty
