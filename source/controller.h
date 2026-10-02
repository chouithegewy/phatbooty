#pragma once

#include "engine.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"

#include <string>
#include <vector>

namespace PhatBooty {

class Controller : public Steinberg::Vst::EditController, public VSTGUI::VST3EditorDelegate
{
public:
	static Steinberg::FUnknown* createInstance (void*)
	{
		return static_cast<Steinberg::Vst::IEditController*> (new Controller);
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
	                                                  Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

	VSTGUI::CView* createCustomView (VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
	                                 const VSTGUI::IUIDescription* description,
	                                 VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

	// Used by the custom views
	Pattern currentPattern ();
	int playheadStep ();
	int seed ();
	int scaleIndex ();
	void rollDice ();
	int rootNote ();
	std::string exportRiff (); // writes the current riff as .mid; returns its path or "" on failure
	void addLiveView (VSTGUI::CView* view) { liveViews.push_back (view); }
	void removeLiveView (VSTGUI::CView* view);

private:
	double plainValue (int id) { return toPlain (id, getParamNormalized (id)); }

	std::vector<VSTGUI::CView*> liveViews; // redrawn on every parameter change
};

} // namespace PhatBooty
