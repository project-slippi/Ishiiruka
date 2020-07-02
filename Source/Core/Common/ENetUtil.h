// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
#pragma once

#include <enet/enet.h>

namespace ENetUtil
{

void WakeupThread(ENetHost *host);
int ENET_CALLBACK InterceptCallback(ENetHost *host, ENetEvent *event);

// Creates a class that can be used with unique_ptr to clean up a host correctly when it's
// passed between classes
class DestroyableHost
{
  public:
	DestroyableHost(ENetHost *host);
	~DestroyableHost();
	ENetHost *GetHost();

  protected:
	ENetHost *host;
};

} // namespace ENetUtil
