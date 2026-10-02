#include "processor.h"
#include "cids.h"
#include "state.h"

#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace PhatBooty {

//------------------------------------------------------------------------
Processor::Processor ()
{
	setControllerClass (kControllerUID);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::initialize (FUnknown* context)
{
	tresult result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);
	addEventInput (STR16 ("Event In"), 1);
	addEventOutput (STR16 ("MIDI Out"), 1);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                  SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
		return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
	return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& setup)
{
	engine.setSampleRate (setup.sampleRate);
	return AudioEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::setActive (TBool state)
{
	if (state)
		engine.reset ();
	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
void Processor::handleEvent (const Event& e)
{
	switch (e.type)
	{
		case Event::kNoteOnEvent:
			if (e.noteOn.velocity > 0.f)
				engine.noteOn (e.noteOn.pitch, e.noteOn.velocity);
			else
				engine.noteOff (e.noteOn.pitch);
			break;
		case Event::kNoteOffEvent: engine.noteOff (e.noteOff.pitch); break;
		default: break;
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::process (ProcessData& data)
{
	// Parameter changes: the last value in the block wins, which is plenty for a bass synth.
	if (auto* changes = data.inputParameterChanges)
	{
		const int32 count = changes->getParameterCount ();
		for (int32 i = 0; i < count; ++i)
		{
			auto* queue = changes->getParameterData (i);
			if (!queue)
				continue;
			const int32 points = queue->getPointCount ();
			ParamValue value;
			int32 offset;
			if (points > 0 && queue->getPoint (points - 1, offset, value) == kResultTrue)
				engine.setParamNormalized (static_cast<int> (queue->getParameterId ()), value);
		}
	}

	if (data.numOutputs == 0 || data.numSamples <= 0)
		return kResultOk;

	Engine::Transport transport;
	if (const auto* ctx = data.processContext)
	{
		transport.playing = (ctx->state & ProcessContext::kPlaying) != 0;
		if (ctx->state & ProcessContext::kTempoValid)
			transport.tempo = ctx->tempo;
		if (ctx->state & ProcessContext::kProjectTimeMusicValid)
			transport.ppq = ctx->projectTimeMusic;
		else
			transport.playing = false; // can't sync without a musical position
	}
	engine.beginBlock (transport);

	AudioBusBuffers& out = data.outputs[0];
	float* left = out.channelBuffers32[0];
	float* right = out.numChannels > 1 ? out.channelBuffers32[1] : nullptr;

	// Split rendering at each event so notes land sample-accurately.
	int32 pos = 0;
	if (auto* events = data.inputEvents)
	{
		const int32 count = events->getEventCount ();
		for (int32 i = 0; i < count; ++i)
		{
			Event e;
			if (events->getEvent (i, e) != kResultOk)
				continue;
			const int32 at = std::clamp (e.sampleOffset, pos, data.numSamples);
			if (at > pos)
			{
				engine.render (left, right, pos, at);
				pos = at;
			}
			handleEvent (e);
		}
	}
	if (pos < data.numSamples)
		engine.render (left, right, pos, data.numSamples);

	out.silenceFlags = 0;

	// Mirror every played note to the MIDI output bus.
	if (auto* outEvents = data.outputEvents)
	{
		for (int i = 0; i < engine.numMidiOut (); ++i)
		{
			const auto& m = engine.midiOut (i);
			Event e {};
			e.busIndex = 0;
			e.sampleOffset = std::clamp (m.sampleOffset, 0, data.numSamples - 1);
			if (m.on)
			{
				e.type = Event::kNoteOnEvent;
				e.noteOn = {0, static_cast<int16> (m.pitch), 0.f, m.velocity, 0, -1};
			}
			else
			{
				e.type = Event::kNoteOffEvent;
				e.noteOff = {0, static_cast<int16> (m.pitch), 0.f, -1, 0.f};
			}
			outEvents->addEvent (e);
		}
	}

	// Tell the UI where the playhead is and which key we're in.
	if (auto* outParams = data.outputParameterChanges)
	{
		auto report = [&] (ParamID id, ParamValue value) {
			int32 index, pointIndex;
			if (auto* queue = outParams->addParameterData (id, index))
				return queue->addPoint (0, value, pointIndex) == kResultTrue;
			return false;
		};
		const int step = engine.currentStep ();
		if (step != lastReportedStep && report (kPlayhead, (step + 1) / static_cast<double> (kPlayheadSteps)))
			lastReportedStep = step;
		const int root = engine.rootNote ();
		if (root != lastReportedRoot && report (kRootNote, root / 127.))
			lastReportedRoot = root;
	}
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	double values[kNumParams];
	for (int i = 0; i < kNumParams; ++i)
		values[i] = engine.getParamNormalized (i);
	IBStreamer s (state, kLittleEndian);
	if (!readState (s, values))
		return kResultFalse;
	for (int i = 0; i < kNumParams; ++i)
		engine.setParamNormalized (i, values[i]);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Processor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	double values[kNumParams];
	for (int i = 0; i < kNumParams; ++i)
		values[i] = engine.getParamNormalized (i);
	IBStreamer s (state, kLittleEndian);
	return writeState (s, values) ? kResultOk : kResultFalse;
}

} // namespace PhatBooty
