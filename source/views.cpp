#include "views.h"
#include "controller.h"
#include "engine.h"
#include "filedrag.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgraphicstransform.h"
#include "vstgui/lib/cdropsource.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/dragging.h"
#include "vstgui/lib/events.h"

#include <cstdio>
#include <cstdlib>

using namespace VSTGUI;

namespace PhatBooty {
namespace {

const CColor kField (14, 8, 21);
const CColor kEdge (58, 37, 82);
const CColor kBeatShade (255, 255, 255, 10);
const CColor kText (244, 236, 255);
const CColor kTextDim (155, 135, 181);
const CColor kPink (255, 61, 139);
const CColor kOrange (255, 181, 46);
const CColor kCyan (61, 232, 255);
const CColor kGhost (120, 90, 160);

void fillRoundRect (CDrawContext* context, const CRect& r, CCoord radius, const CColor& fill,
                    const CColor* frame = nullptr)
{
	SharedPointer<CGraphicsPath> path = owned (context->createRoundRectGraphicsPath (r, radius));
	if (!path)
		return;
	context->setFillColor (fill);
	context->drawGraphicsPath (path, CDrawContext::kPathFilled);
	if (frame)
	{
		context->setFrameColor (*frame);
		context->setLineWidth (1.);
		context->drawGraphicsPath (path, CDrawContext::kPathStroked);
	}
}

const char* intervalName (int semis)
{
	static const char* names[] = {"R", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7"};
	return names[((semis % 12) + 12) % 12];
}

} // namespace

//------------------------------------------------------------------------
PatternView::PatternView (const CRect& size, Controller* controller)
: CView (size), controller (controller)
{
}

bool PatternView::attached (CView* parent)
{
	controller->addLiveView (this);
	return CView::attached (parent);
}

bool PatternView::removed (CView* parent)
{
	controller->removeLiveView (this);
	return CView::removed (parent);
}

void PatternView::draw (CDrawContext* context)
{
	context->setDrawMode (kAntiAliasing);
	const CRect bounds = getViewSize ();
	fillRoundRect (context, bounds, 10., kField, &kEdge);

	const Pattern pattern = controller->currentPattern ();
	const int playhead = controller->playheadStep ();
	const int bars = pattern.length / kStepsPerBar;

	// Caption
	char caption[128];
	std::snprintf (caption, sizeof (caption), "PATTERN  %d BAR%s  /  SEED %d  /  %s", bars,
	               bars > 1 ? "S" : "", controller->seed (), kScaleNames[controller->scaleIndex ()]);
	context->setFont (kNormalFontSmall);
	context->setFontColor (kTextDim);
	context->drawString (caption, CRect (bounds.left + 14, bounds.top + 6, bounds.right - 14, bounds.top + 22),
	                     kLeftText);
	// Status sits left of the Preview controls that the layout overlays in the top-right corner.
	const char* status = playhead >= 0 ? "PLAYING" : (controller->previewOn () ? "PRESS PLAY" : "HOLD A KEY");
	context->setFontColor (playhead >= 0 ? kCyan : kTextDim);
	context->drawString (status, CRect (bounds.left + 14, bounds.top + 6, bounds.right - 196, bounds.top + 22),
	                     kRightText);

	CRect grid (bounds.left + 12, bounds.top + 26, bounds.right - 12, bounds.bottom - 10);
	const CCoord colW = grid.getWidth () / kStepsPerBar;
	const CCoord rowH = grid.getHeight () / bars;
	const CCoord cellH = std::min (rowH - 4., 44.);

	// Beat shading: every other beat gets a subtle band.
	for (int beat = 1; beat < 4; beat += 2)
	{
		CRect band (grid.left + beat * 4 * colW, grid.top, grid.left + (beat + 1) * 4 * colW, grid.bottom);
		context->setFillColor (kBeatShade);
		context->drawRect (band, kDrawFilled);
	}

	auto cellRect = [&] (int i) {
		const int row = i / kStepsPerBar;
		const int col = i % kStepsPerBar;
		const CCoord cy = grid.top + row * rowH + rowH / 2.;
		return CRect (grid.left + col * colW + 2, cy - cellH / 2., grid.left + (col + 1) * colW - 2, cy + cellH / 2.);
	};

	for (int i = 0; i < pattern.length; ++i)
	{
		const Step& st = pattern.steps[i];
		CRect r = cellRect (i);

		if (i == playhead)
		{
			CRect glow = r;
			glow.extend (4, 4);
			fillRoundRect (context, glow, 7., CColor (61, 232, 255, 70));
		}

		if (!st.on)
		{
			const CPoint c = r.getCenter ();
			context->setFillColor (kEdge);
			context->drawEllipse (CRect (c.x - 2, c.y - 2, c.x + 2, c.y + 2), kDrawFilled);
			continue;
		}

		if (st.ghost)
		{
			CRect g = r;
			g.inset (r.getWidth () * 0.22, r.getHeight () * 0.28);
			fillRoundRect (context, g, 4., kGhost);
			continue;
		}

		fillRoundRect (context, r, 5., st.accent ? kPink : kOrange);
		if (st.semis >= 12)
		{
			// Octave pop: a bright cap on top of the cell
			CRect cap (r.left + 3, r.top + 3, r.right - 3, r.top + 6);
			fillRoundRect (context, cap, 1.5, kText);
		}
		if (st.slide && i + 1 < pattern.length && (i + 1) % kStepsPerBar != 0)
		{
			const CRect next = cellRect (i + 1);
			context->setFrameColor (kCyan);
			context->setLineWidth (3.);
			context->drawLine (CPoint (r.right - 4, r.getCenter ().y), CPoint (next.left + 4, r.getCenter ().y));
		}

		char label[8];
		std::snprintf (label, sizeof (label), "%s%s", intervalName (st.semis), st.semis >= 12 ? "'" : "");
		context->setFont (kNormalFontSmall);
		context->setFontColor (kField);
		context->drawString (label, r, kCenterText);
	}
}

//------------------------------------------------------------------------
DiceButton::DiceButton (const CRect& size, Controller* controller)
: CView (size), controller (controller)
{
	setWantsFocus (false);
	setTooltipText ("Roll a new groove");
}

bool DiceButton::attached (CView* parent)
{
	controller->addLiveView (this);
	return CView::attached (parent);
}

bool DiceButton::removed (CView* parent)
{
	if (timer)
	{
		timer->stop ();
		timer = nullptr;
	}
	controller->removeLiveView (this);
	return CView::removed (parent);
}

void DiceButton::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	controller->rollDice ();
	framesLeft = 14;
	if (!timer)
		timer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) { tick (); }, 16, true);
	event.consumed = true;
}

void DiceButton::onMouseEnterEvent (MouseEnterEvent& event)
{
	hovered = true;
	invalid ();
	event.consumed = true;
}

void DiceButton::onMouseExitEvent (MouseExitEvent& event)
{
	hovered = false;
	invalid ();
	event.consumed = true;
}

void DiceButton::tick ()
{
	if (--framesLeft <= 0)
	{
		framesLeft = 0;
		angle = 0.;
		if (timer)
		{
			timer->stop ();
			timer = nullptr;
		}
	}
	else
	{
		tumbleFace = 1 + std::rand () % 6;
		angle += 360. / 14.;
	}
	invalid ();
}

void DiceButton::draw (CDrawContext* context)
{
	context->setDrawMode (kAntiAliasing);
	const CRect bounds = getViewSize ();
	const CPoint center = bounds.getCenter ();
	const int face = framesLeft > 0 ? tumbleFace : controller->seed () % 6 + 1;

	CDrawContext::Transform transform (*context, CGraphicsTransform ().rotate (angle, center));

	CRect body = bounds;
	body.inset (7, 7);
	CRect shadow = body;
	shadow.offset (0, 3);
	fillRoundRect (context, shadow, 9., CColor (0, 0, 0, 120));
	fillRoundRect (context, body, 9., hovered || framesLeft > 0 ? kPink : kText);

	// Pip layout on a 3x3 grid
	static const bool pips[6][9] = {
	    {0, 0, 0, 0, 1, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 0, 0, 0, 1, 0, 0, 0, 1},
	    {1, 0, 1, 0, 0, 0, 1, 0, 1}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {1, 0, 1, 1, 0, 1, 1, 0, 1},
	};
	const CCoord step = body.getWidth () / 3.6;
	const CCoord r = body.getWidth () / 11.;
	context->setFillColor (hovered || framesLeft > 0 ? kText : kField);
	for (int i = 0; i < 9; ++i)
	{
		if (!pips[face - 1][i])
			continue;
		const CCoord x = center.x + (i % 3 - 1) * step;
		const CCoord y = center.y + (i / 3 - 1) * step;
		context->drawEllipse (CRect (x - r, y - r, x + r, y + r), kDrawFilled);
	}
}


//------------------------------------------------------------------------
MidiDragView::MidiDragView (const CRect& size, Controller* controller)
: CView (size), controller (controller)
{
	setWantsFocus (false);
	setTooltipText ("Drag onto your DAW timeline to drop this riff as MIDI");
}

MidiDragView::~MidiDragView () = default;

bool MidiDragView::removed (CView* parent)
{
	stopTimer ();
	drag.reset ();
	return CView::removed (parent);
}

void MidiDragView::stopTimer ()
{
	if (timer)
	{
		timer->stop ();
		timer = nullptr;
	}
}

void MidiDragView::flash (const char* text)
{
	status = text;
	statusFrames = 90; // ~1.5 s at 60 Hz
	if (!timer)
	{
		timer = makeOwned<CVSTGUITimer> (
		    [this] (CVSTGUITimer*) {
			    if (drag)
			    {
				    drag->poll ();
				    if (drag->isDone ())
					    drag.reset ();
			    }
			    if (statusFrames > 0 && --statusFrames == 0)
			    {
				    status = nullptr;
				    invalid ();
			    }
			    if (!drag && !dragging && statusFrames == 0)
				    stopTimer ();
		    },
		    16, true);
	}
	invalid ();
}

void MidiDragView::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	pressPos = event.mousePosition;
	armed = true;
	event.consumed = true;
}

void MidiDragView::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!armed || dragging)
		return;
	const CPoint d = event.mousePosition - pressPos;
	if (d.x * d.x + d.y * d.y < 16.)
		return;
	startDrag ();
	event.consumed = true;
}

void MidiDragView::startDrag ()
{
	const std::string path = controller->exportRiff ();
	if (path.empty ())
	{
		armed = false;
		flash ("ERROR");
		return;
	}

	drag = FileDragSource::create ();
	if (drag)
	{
		if (!drag->begin (path))
		{
			drag.reset ();
			armed = false;
			flash ("ERROR");
			return;
		}
		dragging = true;
		flash ("DRAGGING");
		return;
	}

	// Platforms where VSTGUI has a native drag source
	armed = false;
	if (auto* frame = getFrame ())
	{
		auto source = CDropSource::create (path.data (), static_cast<uint32_t> (path.size () + 1),
		                                   IDataPackage::kFilePath);
		frame->doDrag (DragDescription (source), nullptr);
	}
}

void MidiDragView::onMouseUpEvent (MouseUpEvent& event)
{
	if (!armed)
		return;
	armed = false;
	event.consumed = true;
	if (dragging)
	{
		dragging = false;
		drag->drop ();
		flash ("DROPPED");
		return;
	}
	// Plain click: save the riff so it can be found in the riffs folder.
	const std::string path = controller->exportRiff ();
	setTooltipText ((path.empty () ? std::string ("Could not save riff") : "Saved " + path).c_str ());
	flash (path.empty () ? "ERROR" : "SAVED");
}

void MidiDragView::onMouseEnterEvent (MouseEnterEvent& event)
{
	hovered = true;
	invalid ();
	event.consumed = true;
}

void MidiDragView::onMouseExitEvent (MouseExitEvent& event)
{
	hovered = false;
	invalid ();
	event.consumed = true;
}

void MidiDragView::draw (CDrawContext* context)
{
	context->setDrawMode (kAntiAliasing);
	CRect r = getViewSize ();
	const bool active = hovered || dragging;
	fillRoundRect (context, r, 8., active ? CColor (61, 232, 255, 40) : CColor (14, 8, 21), &kEdge);

	// Grip: two columns of dots on the left
	context->setFillColor (active ? kCyan : kTextDim);
	for (int row = 0; row < 3; ++row)
		for (int col = 0; col < 2; ++col)
		{
			const CCoord x = r.left + 12 + col * 6;
			const CCoord y = r.getCenter ().y - 6 + row * 6;
			context->drawEllipse (CRect (x - 1.5, y - 1.5, x + 1.5, y + 1.5), kDrawFilled);
		}

	CRect text = r;
	text.left += 22;
	context->setFont (kNormalFontSmall);
	context->setFontColor (status ? kOrange : (active ? kCyan : kText));
	context->drawString (status ? status : "DRAG MIDI", text, kCenterText);
}

} // namespace PhatBooty
