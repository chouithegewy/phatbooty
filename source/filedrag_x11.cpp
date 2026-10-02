#include "filedrag.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace PhatBooty {
namespace {

constexpr long kXdndVersion = 5;
constexpr auto kDropTimeout = std::chrono::seconds (3);

class X11FileDrag : public FileDragSource
{
public:
	~X11FileDrag () override
	{
		if (!display)
			return;
		if (target && (state == State::Dragging || state == State::DropPending))
			sendLeave ();
		XDestroyWindow (display, source);
		XCloseDisplay (display);
	}

	bool init ()
	{
		display = XOpenDisplay (nullptr);
		if (!display)
			return false;
		root = DefaultRootWindow (display);
		XSetWindowAttributes attrs {};
		source = XCreateWindow (display, root, -10, -10, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, 0,
		                        &attrs);
		auto atom = [&] (const char* name) { return XInternAtom (display, name, False); };
		aAware = atom ("XdndAware");
		aProxy = atom ("XdndProxy");
		aEnter = atom ("XdndEnter");
		aPosition = atom ("XdndPosition");
		aStatus = atom ("XdndStatus");
		aLeave = atom ("XdndLeave");
		aDrop = atom ("XdndDrop");
		aFinished = atom ("XdndFinished");
		aSelection = atom ("XdndSelection");
		aActionCopy = atom ("XdndActionCopy");
		aUriList = atom ("text/uri-list");
		aTextPlain = atom ("text/plain");
		aUtf8 = atom ("UTF8_STRING");
		aTargets = atom ("TARGETS");
		return true;
	}

	bool begin (const std::string& path) override
	{
		plainData = path;
		uriData = "file://" + percentEncode (path) + "\r\n";
		XSetSelectionOwner (display, aSelection, source, CurrentTime);
		if (XGetSelectionOwner (display, aSelection) != source)
			return false;
		state = State::Dragging;
		poll ();
		return true;
	}

	void poll () override
	{
		serviceEvents ();
		if (state == State::Dragging)
			trackPointer ();
		if ((state == State::Dropped || state == State::DropPending) &&
		    std::chrono::steady_clock::now () - dropTime > kDropTimeout)
			state = State::Done;
	}

	void drop () override
	{
		if (state != State::Dragging)
			return;
		trackPointer ();
		dropTime = std::chrono::steady_clock::now ();
		if (!target)
			state = State::Done;
		else if (waitingStatus)
			state = State::DropPending; // decide once the target answers our last position
		else
			finishDrop ();
	}

	bool isDone () const override { return state == State::Done; }

private:
	enum class State
	{
		Idle,
		Dragging,
		DropPending,
		Dropped,
		Done
	};

	static std::string percentEncode (const std::string& path)
	{
		static const char* hex = "0123456789ABCDEF";
		std::string out;
		for (unsigned char c : path)
		{
			if (isalnum (c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~')
				out += static_cast<char> (c);
			else
			{
				out += '%';
				out += hex[c >> 4];
				out += hex[c & 15];
			}
		}
		return out;
	}

	// Outermost XdndAware window under the pointer (following XdndProxy if set).
	Window findTarget (int x, int y, Window& proxy, long& version)
	{
		Window w = root;
		for (int depth = 0; depth < 32; ++depth)
		{
			Window child = 0;
			int cx, cy;
			if (!XTranslateCoordinates (display, root, w, x, y, &cx, &cy, &child) || !child)
				return 0;
			w = child;
			proxy = windowProperty (w, aProxy, XA_WINDOW);
			if ((version = awareVersion (proxy ? proxy : w)) > 0)
				return w;
			proxy = 0;
		}
		return 0;
	}

	long awareVersion (Window w)
	{
		const long v = static_cast<long> (windowProperty (w, aAware, XA_ATOM));
		return v >= 3 ? std::min (v, kXdndVersion) : 0;
	}

	unsigned long windowProperty (Window w, Atom property, Atom type)
	{
		Atom actualType;
		int format;
		unsigned long count = 0, remaining;
		unsigned char* data = nullptr;
		unsigned long value = 0;
		if (XGetWindowProperty (display, w, property, 0, 1, False, type, &actualType, &format, &count,
		                        &remaining, &data) == Success &&
		    data && count == 1 && format == 32)
			value = *reinterpret_cast<unsigned long*> (data);
		if (data)
			XFree (data);
		return value;
	}

	void trackPointer ()
	{
		Window r, c;
		int x, y, wx, wy;
		unsigned int mask;
		if (!XQueryPointer (display, root, &r, &c, &x, &y, &wx, &wy, &mask))
			return;
		Window proxy = 0;
		long version = 0;
		const Window w = findTarget (x, y, proxy, version);
		if (w != target)
		{
			if (target)
				sendLeave ();
			target = w;
			targetProxy = proxy;
			targetVersion = version;
			accepted = waitingStatus = pendingPosition = false;
			lastX = lastY = -1;
			if (target)
				sendEnter ();
		}
		if (target && (x != lastX || y != lastY))
		{
			lastX = x;
			lastY = y;
			if (waitingStatus)
				pendingPosition = true;
			else
				sendPosition ();
		}
	}

	void finishDrop ()
	{
		if (accepted)
		{
			send (aDrop, static_cast<long> (source), 0, CurrentTime, 0, 0);
			state = State::Dropped;
		}
		else
		{
			sendLeave ();
			state = State::Done;
		}
	}

	void send (Atom type, long l0, long l1, long l2, long l3, long l4)
	{
		XEvent e {};
		e.xclient.type = ClientMessage;
		e.xclient.display = display;
		e.xclient.window = target;
		e.xclient.message_type = type;
		e.xclient.format = 32;
		e.xclient.data.l[0] = l0;
		e.xclient.data.l[1] = l1;
		e.xclient.data.l[2] = l2;
		e.xclient.data.l[3] = l3;
		e.xclient.data.l[4] = l4;
		XSendEvent (display, targetProxy ? targetProxy : target, False, NoEventMask, &e);
		XFlush (display);
	}

	void sendEnter ()
	{
		send (aEnter, static_cast<long> (source), targetVersion << 24, static_cast<long> (aUriList),
		      static_cast<long> (aTextPlain), static_cast<long> (aUtf8));
	}

	void sendPosition ()
	{
		send (aPosition, static_cast<long> (source), 0, (static_cast<long> (lastX) << 16) | lastY, CurrentTime,
		      static_cast<long> (aActionCopy));
		waitingStatus = true;
	}

	void sendLeave ()
	{
		send (aLeave, static_cast<long> (source), 0, 0, 0, 0);
		target = 0;
	}

	void serviceEvents ()
	{
		while (XPending (display))
		{
			XEvent e;
			XNextEvent (display, &e);
			if (e.type == SelectionRequest)
				answerSelectionRequest (e.xselectionrequest);
			else if (e.type == ClientMessage && e.xclient.message_type == aStatus)
				onStatus (e.xclient);
			else if (e.type == ClientMessage && e.xclient.message_type == aFinished)
				state = State::Done;
		}
	}

	void onStatus (const XClientMessageEvent& msg)
	{
		if (static_cast<Window> (msg.data.l[0]) != target)
			return;
		accepted = (msg.data.l[1] & 1) != 0;
		waitingStatus = false;
		if (state == State::DropPending)
			finishDrop ();
		else if (pendingPosition)
		{
			pendingPosition = false;
			sendPosition ();
		}
	}

	void answerSelectionRequest (const XSelectionRequestEvent& req)
	{
		XEvent reply {};
		XSelectionEvent& sel = reply.xselection;
		sel.type = SelectionNotify;
		sel.display = display;
		sel.requestor = req.requestor;
		sel.selection = req.selection;
		sel.target = req.target;
		sel.time = req.time;
		sel.property = None;

		const Atom property = req.property ? req.property : req.target;
		auto reply8 = [&] (const std::string& data) {
			XChangeProperty (display, req.requestor, property, req.target, 8, PropModeReplace,
			                 reinterpret_cast<const unsigned char*> (data.data ()), static_cast<int> (data.size ()));
			sel.property = property;
		};
		if (req.target == aTargets)
		{
			const Atom targets[] = {aTargets, aUriList, aTextPlain, aUtf8};
			XChangeProperty (display, req.requestor, property, XA_ATOM, 32, PropModeReplace,
			                 reinterpret_cast<const unsigned char*> (targets), 4);
			sel.property = property;
		}
		else if (req.target == aUriList)
			reply8 (uriData);
		else if (req.target == aTextPlain || req.target == aUtf8 || req.target == XA_STRING)
			reply8 (plainData);

		XSendEvent (display, req.requestor, False, NoEventMask, &reply);
		XFlush (display);
	}

	Display* display = nullptr;
	Window root = 0, source = 0;
	Atom aAware, aProxy, aEnter, aPosition, aStatus, aLeave, aDrop, aFinished, aSelection, aActionCopy, aUriList,
	    aTextPlain, aUtf8, aTargets;

	std::string uriData, plainData;
	State state = State::Idle;
	Window target = 0, targetProxy = 0;
	long targetVersion = 0;
	bool accepted = false, waitingStatus = false, pendingPosition = false;
	int lastX = -1, lastY = -1;
	std::chrono::steady_clock::time_point dropTime;
};

} // namespace

std::unique_ptr<FileDragSource> FileDragSource::create ()
{
	auto drag = std::make_unique<X11FileDrag> ();
	if (!drag->init ())
		return nullptr;
	return drag;
}

} // namespace PhatBooty
