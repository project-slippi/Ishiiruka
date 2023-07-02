#pragma once

#ifdef USE_UPNP

#include "Common/CommonTypes.h"

namespace UPnP
{
void TryPortmapping(u16 port);
void TryPortmappingBlocking(u16 port);
void StopPortmapping();
} // namespace UPnP

#endif
