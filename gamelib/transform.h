
#pragma once

// todo refactor the include away
#include "graphics/transform.h"

namespace game
{
    // provide type aliases for now for these types so that we can
    // use them as if they weren't in graphics where they shouldn't
    // be for most of the use in gamelib code. (i.e. not related to
    // graphics in any way)
    // todo: eventually should refactor them out of graphics/ into base/
    using Transform = gfx::Transform;

} // game
