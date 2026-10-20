#pragma once

struct __CFError;

namespace kraft {

// Why the system would not trust a certificate, from the error SecTrustEvaluateWithError gave, in the words the
// Windows build uses
void MacTrustFailureText(__CFError* error, char* out, u64 capacity);

// Returns true when the scroll being handled came from a trackpad or the like, whose deltas are in points
bool MacScrollIsPrecise();

} // namespace kraft
