#include "filedrag.h"

namespace PhatBooty {

// macOS and Windows use VSTGUI's native CFrame::doDrag instead.
std::unique_ptr<FileDragSource> FileDragSource::create ()
{
	return nullptr;
}

} // namespace PhatBooty
