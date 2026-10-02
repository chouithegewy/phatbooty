#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

namespace PhatBooty {

static const Steinberg::FUID kProcessorUID (0x696E8924, 0xBD9E416E, 0x9489205F, 0xC5145D27);
static const Steinberg::FUID kControllerUID (0x6FA5C522, 0x246B405A, 0x84275106, 0x7B5D92C2);

#define PhatBootyVST3Category Steinberg::Vst::PlugType::kInstrumentSynth

} // namespace PhatBooty
