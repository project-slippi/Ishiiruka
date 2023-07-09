#pragma once

#include "Common/CommonTypes.h"

namespace Common
{
void TryPortmapping(u16 port);
void TryPortmappingBlocking(u16 port);
void StopPortmapping();
} // namespace Common
