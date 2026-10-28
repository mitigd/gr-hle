// Debug aids that are not seams.
#pragma once
#include "hlekit/machine.h"

namespace game::support {

// GR_WATCH=1: where the guest is twice a second and how much of the VGA window is lit.
// Called from the 60 Hz timer task.
void DebugWatch(hlekit::Machine& m);

}  // namespace game::support
