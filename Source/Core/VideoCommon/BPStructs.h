// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct BPCmd;

void BPInit();
void BPReload();
void BPWritten(const BPCmd& bp);
