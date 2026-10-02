#include "controller.h"
#include "params.h"
#include "state.h"
#include "views.h"

#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace PhatBooty {
namespace {

// Shows whole numbers with their unit ("60%", "220 ms") instead of "60.0000".
class UnitParameter : public RangeParameter
{
public:
	using RangeParameter::RangeParameter;

	void toString (ParamValue normalized, String128 string) const SMTG_OVERRIDE
	{
		if (info.stepCount > 0)
		{
			RangeParameter::toString (normalized, string);
			return;
		}
		char units[32] = {};
		UString (const_cast<char16*> (info.units), 128).toAscii (units, sizeof (units));
		const double plain = toPlain (normalized);
		char text[64];
		if (std::strcmp (units, "%") == 0)
			std::snprintf (text, sizeof (text), "%.0f%%", plain);
		else
			std::snprintf (text, sizeof (text), "%.0f %s", plain, units);
		UString (string, 128).fromAscii (text);
	}
};

} // namespace

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
	tresult result = EditController::initialize (context);
	if (result != kResultOk)
		return result;

	for (int id = 0; id < kNumParams; ++id)
	{
		const ParamInfo& p = kParams[id];
		UString128 title (p.name);
		UString128 units (p.units);
		if (p.list)
		{
			auto* param = new StringListParameter (title, id);
			for (int i = 0; i <= p.stepCount; ++i)
				param->appendString (UString128 (p.list[i]));
			param->setNormalized (toNormalized (id, p.defaultPlain));
			param->getInfo ().defaultNormalizedValue = toNormalized (id, p.defaultPlain);
			parameters.addParameter (param);
		}
		else
		{
			parameters.addParameter (new UnitParameter (title, id, units, p.minPlain, p.maxPlain,
			                                             p.defaultPlain, p.stepCount));
		}
	}

	parameters.addParameter (STR16 ("Playhead"), nullptr, kPlayheadSteps, 0.,
	                         ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden, kPlayhead);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	double values[kNumParams];
	for (int i = 0; i < kNumParams; ++i)
		values[i] = toNormalized (i, kParams[i].defaultPlain);
	IBStreamer s (state, kLittleEndian);
	if (!readState (s, values))
		return kResultFalse;
	for (int i = 0; i < kNumParams; ++i)
		setParamNormalized (i, values[i]);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::setParamNormalized (ParamID tag, ParamValue value)
{
	const tresult result = EditController::setParamNormalized (tag, value);
	if (result == kResultOk)
		for (auto* view : liveViews)
			view->invalid ();
	return result;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API Controller::createView (FIDString name)
{
	if (FIDStringsEqual (name, ViewType::kEditor))
		return new VSTGUI::VST3Editor (this, "view", "phatbooty.uidesc");
	return nullptr;
}

//------------------------------------------------------------------------
VSTGUI::CView* Controller::createCustomView (VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                             const VSTGUI::IUIDescription*, VSTGUI::VST3Editor*)
{
	VSTGUI::CPoint origin, size;
	attributes.getPointAttribute ("origin", origin);
	attributes.getPointAttribute ("size", size);
	const VSTGUI::CRect rect (origin, size);

	const VSTGUI::UTF8StringView view (name);
	if (view == "PatternView")
		return new PatternView (rect, this);
	if (view == "DiceButton")
		return new DiceButton (rect, this);
	return nullptr;
}

//------------------------------------------------------------------------
Pattern Controller::currentPattern ()
{
	double plain[kNumParams];
	for (int i = 0; i < kNumParams; ++i)
		plain[i] = plainValue (i);
	Pattern p;
	p.generate (plain);
	return p;
}

int Controller::playheadStep ()
{
	return static_cast<int> (getParamNormalized (kPlayhead) * kPlayheadSteps + 0.5) - 1;
}

int Controller::seed () { return static_cast<int> (plainValue (kSeed)); }

int Controller::scaleIndex () { return std::clamp (static_cast<int> (plainValue (kScale)), 0, kNumScales - 1); }

void Controller::rollDice ()
{
	static std::mt19937 rng {std::random_device {}()};
	const int current = seed ();
	const int range = static_cast<int> (kParams[kSeed].maxPlain) + 1;
	int next;
	do
		next = static_cast<int> (rng () % range);
	while (next == current);

	const ParamValue value = toNormalized (kSeed, next);
	beginEdit (kSeed);
	setParamNormalized (kSeed, value);
	performEdit (kSeed, value);
	endEdit (kSeed);
}

void Controller::removeLiveView (VSTGUI::CView* view)
{
	liveViews.erase (std::remove (liveViews.begin (), liveViews.end (), view), liveViews.end ());
}

} // namespace PhatBooty
