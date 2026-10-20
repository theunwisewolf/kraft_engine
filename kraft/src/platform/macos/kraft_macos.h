#pragma once

struct __CFError;

namespace kraft {

struct Window;

// Why the system would not trust a certificate, from the error SecTrustEvaluateWithError gave, in the words the
// Windows build uses
void MacTrustFailureText(__CFError* error, char* out, u64 capacity);

// Returns true when the scroll being handled came from a trackpad or the like, whose deltas are in points
bool MacScrollIsPrecise();

// Gives the window's title bar to the app: its content runs under it, the window's own buttons stay at its top left,
// and the caption Window::SetCaptionRegion describes moves the window. Width and height are the content's, in points.
void MacUseCustomTitleBar(Window* window, i32 width, i32 height);

} // namespace kraft
