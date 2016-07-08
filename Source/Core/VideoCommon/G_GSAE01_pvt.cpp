// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#include "VideoCommon/G_GSAE01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_GSAE01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx0_3_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_
// num_verts= 316201745
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20957902454934] = TemplatedLoader<0x301, 0x00032300u, 0x40e0e007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20957902454934] = TemplatedLoader<0, 0x00032300u, 0x40e0e007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T7_mtx1_0_Inv_u8_
// num_verts= 310430468
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21238217021807] = TemplatedLoader<0x301, 0x00030f81u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21238217021807] = TemplatedLoader<0, 0x00030f81u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T1_mtx1_1_Inv_s16_
// num_verts= 263714457
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21237893032288] = TemplatedLoader<0x301, 0x00030f03u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21237893032288] = TemplatedLoader<0, 0x00030f03u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_C0_1_Dir_4444_
// num_verts= 215111129
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20460496760982] = TemplatedLoader<0x301, 0x00001300u, 0x4000e007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20460496760982] = TemplatedLoader<0, 0x00001300u, 0x4000e007u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T6_mtx1_0_Inv_u8_T7_mtx1_0_Inv_u8_
// num_verts= 19145054
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21238381588399] = TemplatedLoader<0x301, 0x00030fc1u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21238381588399] = TemplatedLoader<0, 0x00030fc1u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T1_mtx0_1_I16_s16_
// num_verts= 18463890
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23260082172078] = TemplatedLoader<0x301, 0x000f0f01u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23260082172078] = TemplatedLoader<0, 0x000f0f01u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_T0_mtx0_1_I8_s16_
// num_verts= 9164116
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20766587185046] = TemplatedLoader<0x301, 0x00020200u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20766587185046] = TemplatedLoader<0, 0x00020200u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_T0_mtx0_1_I8_s16_
// num_verts= 7422576
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21060792444822] = TemplatedLoader<0x301, 0x00020200u, 0x40e00007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21060792444822] = TemplatedLoader<0, 0x00020200u, 0x40e00007u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_
// num_verts= 7273824
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21237887888623] = TemplatedLoader<0x301, 0x00030f01u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21237887888623] = TemplatedLoader<0, 0x00030f01u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx1_1_I8_s16_
// num_verts= 5941354
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21066080366575] = TemplatedLoader<0x301, 0x00020a01u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21066080366575] = TemplatedLoader<0, 0x00020a01u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx0_1_I16_s16_
// num_verts= 2785028
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21237885317270] = TemplatedLoader<0x301, 0x00030f00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21237885317270] = TemplatedLoader<0, 0x00030f00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_Dir_s16_T0_mtx0_1_Dir_flt_
// num_verts= 2256156
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20970340880022] = TemplatedLoader<0x301, 0x00010100u, 0x41200007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20970340880022] = TemplatedLoader<0, 0x00010100u, 0x41200007u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T1_mtx0_1_I16_s16_T7_mtx1_0_Inv_u8_
// num_verts= 2084914
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23260411305262] = TemplatedLoader<0x301, 0x000f0f81u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23260411305262] = TemplatedLoader<0, 0x000f0f81u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_
// num_verts= 1941912
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21082933258134] = TemplatedLoader<0x301, 0x00022200u, 0x40e0e007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21082933258134] = TemplatedLoader<0, 0x00022200u, 0x40e0e007u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I16_s8_T0_mtx0_1_I8_s16_
// num_verts= 1213232
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21068710860694] = TemplatedLoader<0x301, 0x00020e00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21068710860694] = TemplatedLoader<0, 0x00020e00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_s16_Nrm_0_0_Dir_u8_
// num_verts= 737624
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20155999482518] = TemplatedLoader<0x301, 0x00000500u, 0x40000007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20155999482518] = TemplatedLoader<0, 0x00000500u, 0x40000007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T1_mtx1_1_I16_s16_
// num_verts= 564972
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23260087314784] = TemplatedLoader<0x301, 0x000f0f03u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23260087314784] = TemplatedLoader<0, 0x000f0f03u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_
// num_verts= 261724
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22980096738389] = TemplatedLoader<0x301, 0x000f2300u, 0x40e0e007u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22980096738389] = TemplatedLoader<0, 0x000f2300u, 0x40e0e007u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_
// num_verts= 257600
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21066077795222] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21066077795222] = TemplatedLoader<0, 0x00020a00u, 0x40e00407u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx0_1_I8_s16_
// num_verts= 139464
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20775163867286] = TemplatedLoader<0x301, 0x00020f00u, 0x40e00407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20775163867286] = TemplatedLoader<0, 0x00020f00u, 0x40e00407u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_T0_mtx0_1_I8_s16_
// num_verts= 85432
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20767245451414] = TemplatedLoader<0x301, 0x00020300u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20767245451414] = TemplatedLoader<0, 0x00020300u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx1_1_I16_s16_T1_mtx0_1_I16_s16_T6_mtx1_0_Inv_u8_T7_mtx1_0_Inv_u8_
// num_verts= 55356
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23260575871854] = TemplatedLoader<0x301, 0x000f0fc1u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23260575871854] = TemplatedLoader<0, 0x000f0fc1u, 0x40e00407u, 0x80000007u, 0x00000000u>;
	}
	// P_mtx1_3_Dir_s16_C0_1_Dir_8888_T0_mtx0_1_Dir_s16_
// num_verts= 18400
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20903841562262] = TemplatedLoader<0x301, 0x00011100u, 0x40e16007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20903841562262] = TemplatedLoader<0, 0x00011100u, 0x40e16007u, 0x80000000u, 0x00000000u>;
	}
}
