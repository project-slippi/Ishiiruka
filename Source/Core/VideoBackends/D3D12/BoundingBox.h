// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "VideoBackends/D3D12/D3DBase.h"

namespace DX12
{

class BBox
{
public:
	static D3D12_GPU_DESCRIPTOR_HANDLE GetUAV();
	static void Init();
	static void Shutdown();

	static void Set(int index, int value);
	static int Get(int index);
};

};
