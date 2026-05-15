#pragma once

// The per-protocol <Proto>Result global and parameterless initResult()
// used to live here. They were collapsed into a single project-wide
// CCBenchResults vector and initResult(size_t) in #101 — see
// include/result.hh at the repo root. This file is kept as a no-op
// forwarder so existing `#include "include/result.hh"` lines in the
// entry points keep working without churn.
#include "../../../include/result.hh"
