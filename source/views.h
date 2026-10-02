// Custom VSTGUI views for the editor: the live pattern display and the dice button.
#pragma once

#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"

#include <functional>

namespace PhatBooty {

class Controller;

// Draws the current groove as a step grid (one row per bar) with the playhead.
class PatternView : public VSTGUI::CView
{
public:
	PatternView (const VSTGUI::CRect& size, Controller* controller);

	void draw (VSTGUI::CDrawContext* context) override;
	bool attached (VSTGUI::CView* parent) override;
	bool removed (VSTGUI::CView* parent) override;

private:
	Controller* controller;
};

// Click to roll a new seed; the die tumbles for a moment and lands on a face derived from the seed.
class DiceButton : public VSTGUI::CView
{
public:
	DiceButton (const VSTGUI::CRect& size, Controller* controller);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseEnterEvent (VSTGUI::MouseEnterEvent& event) override;
	void onMouseExitEvent (VSTGUI::MouseExitEvent& event) override;
	bool attached (VSTGUI::CView* parent) override;
	bool removed (VSTGUI::CView* parent) override;

private:
	void tick ();

	Controller* controller;
	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
	int framesLeft = 0;
	int tumbleFace = 1;
	double angle = 0.;
	bool hovered = false;
};

} // namespace PhatBooty
