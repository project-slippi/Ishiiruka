// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_SPDE52_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_SPDE52_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_s8_T0_mtx0_1_I16_s16_
// num_verts= 166080810
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20943680095032] = TemplatedLoader<0x301, 0x00030f00u, 0x40e00409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20943680095032] = TemplatedLoader<0, 0x00030f00u, 0x40e00409u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_
// num_verts= 123416900
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22957955962615] = TemplatedLoader<0x301, 0x000f0300u, 0x40e00009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22957955962615] = TemplatedLoader<0, 0x000f0300u, 0x40e00009u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
// num_verts= 65759488
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22114716745207] = TemplatedLoader<0x301, 0x000a0200u, 0x40e00009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22114716745207] = TemplatedLoader<0, 0x000a0200u, 0x40e00009u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_
// num_verts= 42878062
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20154682987320] = TemplatedLoader<0x301, 0x00000300u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20154682987320] = TemplatedLoader<0, 0x00000300u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_
// num_verts= 23514317
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22991244060407] = TemplatedLoader<0x301, 0x000f3300u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22991244060407] = TemplatedLoader<0, 0x000f3300u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
// num_verts= 12236002
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22137472581111] = TemplatedLoader<0x301, 0x000a2200u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22137472581111] = TemplatedLoader<0, 0x000a2200u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_s8_C0_1_I16_8888_T0_mtx0_1_I16_s16_
// num_verts= 5036207
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20976968192824] = TemplatedLoader<0x301, 0x00033f00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20976968192824] = TemplatedLoader<0, 0x00033f00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_
// num_verts= 2678223
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31080021681399] = TemplatedLoader<0x301, 0x003f3300u, 0x40e16009u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31080021681399] = TemplatedLoader<0, 0x003f3300u, 0x40e16009u, 0x00000e07u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_T0_mtx0_1_I8_s16_
// num_verts= 2199774
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20766587222584] = TemplatedLoader<0x301, 0x00020200u, 0x40e00009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20766587222584] = TemplatedLoader<0, 0x00020200u, 0x40e00009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_s8_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_
// num_verts= 1798654
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22965874378487] = TemplatedLoader<0x301, 0x000f0f00u, 0x40e00409u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22965874378487] = TemplatedLoader<0, 0x000f0f00u, 0x40e00409u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
// num_verts= 1065364
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22120002095607] = TemplatedLoader<0x301, 0x000a0a00u, 0x40e00409u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22120002095607] = TemplatedLoader<0, 0x000a0a00u, 0x40e00409u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_
// num_verts= 676915
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27507235322871] = TemplatedLoader<0x301, 0x002a0200u, 0x40e00009u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27507235322871] = TemplatedLoader<0, 0x002a0200u, 0x40e00009u, 0x00000e07u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_
// num_verts= 421736
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771872572984] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771872572984] = TemplatedLoader<0, 0x00020a00u, 0x40e00409u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_T0_mtx0_1_I8_s16_
// num_verts= 255296
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21060792482360] = TemplatedLoader<0x301, 0x00020200u, 0x40e00009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21060792482360] = TemplatedLoader<0, 0x00020200u, 0x40e00009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_
// num_verts= 119460
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31046733583607] = TemplatedLoader<0x301, 0x003f0300u, 0x40e00009u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31046733583607] = TemplatedLoader<0, 0x003f0300u, 0x40e00009u, 0x00000e07u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
// num_verts= 108237
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22431677840887] = TemplatedLoader<0x301, 0x000a2200u, 0x40e16009u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22431677840887] = TemplatedLoader<0, 0x000a2200u, 0x40e16009u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_
// num_verts= 31445
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27529991158775] = TemplatedLoader<0x301, 0x002a2200u, 0x40e16009u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27529991158775] = TemplatedLoader<0, 0x002a2200u, 0x40e16009u, 0x00000e07u, 0x00000000u>;
	}
}
