// Dragging a file out of the plug-in window into another app (e.g. a .mid onto a DAW timeline).
#pragma once

#include <memory>
#include <string>

namespace PhatBooty {

// VSTGUI implements drag sources on macOS/Windows but not on Linux, so on X11 we speak XDND
// ourselves on a private display connection. create() returns nullptr where this isn't needed.
class FileDragSource
{
public:
	static std::unique_ptr<FileDragSource> create ();
	virtual ~FileDragSource () = default;

	virtual bool begin (const std::string& path) = 0;
	// Call regularly (~60 Hz): tracks the window under the pointer and answers data requests.
	virtual void poll () = 0;
	// The mouse button was released.
	virtual void drop () = 0;
	// The exchange is over (dropped, rejected or timed out) and the object can be destroyed.
	virtual bool isDone () const = 0;
};

} // namespace PhatBooty
