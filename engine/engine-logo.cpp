

#include <cstddef>

namespace {
    const unsigned char DetonatorLOGO[] = {
#include "engine/engine-logo.h"
    };
} // namespace

const unsigned char* GetEngineLogoData()
{
    return DetonatorLOGO;
}

size_t GetEngineLogoDataSize()
{
    return sizeof(DetonatorLOGO);
}