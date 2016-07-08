// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_RBUP08_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_RBUP08_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_
// num_verts= 94979596
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21159200864406] = TemplatedLoader<0x301, 0x00030f00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21159200864406] = TemplatedLoader<0, 0x00030f00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_
// num_verts= 64559916
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20987393342358] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20987393342358] = TemplatedLoader<0, 0x00020a00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 46127637
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004107214565] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004107214565] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_
// num_verts= 35671630
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22716059889125] = TemplatedLoader<0x301, 0x000e0b00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22716059889125] = TemplatedLoader<0, 0x000e0b00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_
// num_verts= 20626536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22965932036581] = TemplatedLoader<0x301, 0x000f0f00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22965932036581] = TemplatedLoader<0, 0x000f0f00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_T1_mtx1_1_I16_u16_T2_mtx0_1_I16_u16_
// num_verts= 20412465
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31270177770997] = TemplatedLoader<0x301, 0x003f0f02u, 0x40a00c07u, 0x80000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31270177770997] = TemplatedLoader<0, 0x003f0f02u, 0x40a00c07u, 0x80000a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 17681599
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004179042021] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004179042021] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 15102100
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004178761445] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x001c0205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004178761445] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x001c0205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 14338543
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49003963279077] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00040205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49003963279077] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00040205u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_s16_T1_mtx1_1_I16_s16_T2_mtx0_1_I16_s16_
// num_verts= 13454402
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31348900803335] = TemplatedLoader<0x301, 0x003f0f02u, 0x40e00c07u, 0x80000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31348900803335] = TemplatedLoader<0, 0x003f0f02u, 0x40e00c07u, 0x80000e07u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_
// num_verts= 12865978
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22963298971109] = TemplatedLoader<0x301, 0x000f0b00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22963298971109] = TemplatedLoader<0, 0x000f0b00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_
// num_verts= 12543279
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855439589] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855439589] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_
// num_verts= 12185127
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22794782780901] = TemplatedLoader<0x301, 0x000e0b00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22794782780901] = TemplatedLoader<0, 0x000e0b00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_T3_mtx0_1_I16_s16_
// num_verts= 11482198
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63160197285861] = TemplatedLoader<0x301, 0x00fe0b00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63160197285861] = TemplatedLoader<0, 0x00fe0b00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I8_flt_
// num_verts= 11019426
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21148149714744] = TemplatedLoader<0x301, 0x00020f00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21148149714744] = TemplatedLoader<0, 0x00020f00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 10867396
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082830106341] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082830106341] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 10561332
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49678028039909] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40a01009u, 0x00040205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49678028039909] = TemplatedLoader<0, 0x00ae0a00u, 0x40a01009u, 0x00040205u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u8_T1_mtx1_1_I16_u8_T2_mtx0_1_I16_u8_
// num_verts= 9936810
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31112731706321] = TemplatedLoader<0x301, 0x003f0f02u, 0x40200c07u, 0x80000201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31112731706321] = TemplatedLoader<0, 0x003f0f02u, 0x40200c07u, 0x80000201u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_
// num_verts= 9280523
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693865605944] = TemplatedLoader<0x301, 0x00020b00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693865605944] = TemplatedLoader<0, 0x00020b00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_s16_
// num_verts= 8675688
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30973353700325] = TemplatedLoader<0x301, 0x003f0b00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30973353700325] = TemplatedLoader<0, 0x003f0b00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_
// num_verts= 8656956
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20865014861624] = TemplatedLoader<0x301, 0x00030f00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20865014861624] = TemplatedLoader<0, 0x00030f00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_
// num_verts= 7381220
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22041336861925] = TemplatedLoader<0x301, 0x000a0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22041336861925] = TemplatedLoader<0, 0x000a0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_T3_mtx0_1_I16_s16_
// num_verts= 7141316
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63331346541541] = TemplatedLoader<0x301, 0x00ff0f00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63331346541541] = TemplatedLoader<0, 0x00ff0f00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_
// num_verts= 7014047
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22120059753701] = TemplatedLoader<0x301, 0x000a0a00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22120059753701] = TemplatedLoader<0, 0x000a0a00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_s16_
// num_verts= 6901614
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28107920200421] = TemplatedLoader<0x301, 0x002e0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28107920200421] = TemplatedLoader<0, 0x002e0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_
// num_verts= 6769270
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22887209144805] = TemplatedLoader<0x301, 0x000f0f00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22887209144805] = TemplatedLoader<0, 0x000f0f00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_T3_mtx0_1_I16_u16_
// num_verts= 5899848
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63238848350181] = TemplatedLoader<0x301, 0x00fe0b00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63238848350181] = TemplatedLoader<0, 0x00fe0b00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_flt_
// num_verts= 5809236
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21019827579704] = TemplatedLoader<0x301, 0x00030b00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21019827579704] = TemplatedLoader<0, 0x00030b00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_s16_
// num_verts= 5579970
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30804837510117] = TemplatedLoader<0x301, 0x003e0b00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30804837510117] = TemplatedLoader<0, 0x003e0b00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 5316220
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49678243802853] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49678243802853] = TemplatedLoader<0, 0x00ae0a00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_
// num_verts= 5115936
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22794124514533] = TemplatedLoader<0x301, 0x000e0a00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22794124514533] = TemplatedLoader<0, 0x000e0a00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I8_u16_
// num_verts= 4792368
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20696498671416] = TemplatedLoader<0x301, 0x00020f00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20696498671416] = TemplatedLoader<0, 0x00020f00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_T3_mtx0_1_I8_s16_
// num_verts= 4604600
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[52375161112549] = TemplatedLoader<0x301, 0x00be0b00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[52375161112549] = TemplatedLoader<0, 0x00be0b00u, 0x40a01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 4551810
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49849321231077] = TemplatedLoader<0x301, 0x00af0e00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49849321231077] = TemplatedLoader<0, 0x00af0e00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 4534368
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49846688165605] = TemplatedLoader<0x301, 0x00af0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49846688165605] = TemplatedLoader<0, 0x00af0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I16_s16_
// num_verts= 4156698
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[60711177324517] = TemplatedLoader<0x301, 0x00ef0b00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[60711177324517] = TemplatedLoader<0, 0x00ef0b00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_
// num_verts= 3886668
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21159206008345] = TemplatedLoader<0x301, 0x00030f02u, 0x40a00c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21159206008345] = TemplatedLoader<0, 0x00030f02u, 0x40a00c07u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 3503755
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082901933797] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082901933797] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u16_
// num_verts= 3463680
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004106933989] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00140205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004106933989] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00140205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 3308032
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49678243522277] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40a01009u, 0x001c0205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49678243522277] = TemplatedLoader<0, 0x00ae0a00u, 0x40a01009u, 0x001c0205u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 3211692
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49757553133541] = TemplatedLoader<0x301, 0x00ae0b00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49757553133541] = TemplatedLoader<0, 0x00ae0b00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u8_
// num_verts= 3166777
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20829947558806] = TemplatedLoader<0x301, 0x00020a00u, 0x40200c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20829947558806] = TemplatedLoader<0, 0x00020a00u, 0x40200c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 2938572
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49926141151205] = TemplatedLoader<0x301, 0x00af0b00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49926141151205] = TemplatedLoader<0, 0x00af0b00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 2923636
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004107214017] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00140a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004107214017] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00140a01u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_
// num_verts= 2891384
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21158542598038] = TemplatedLoader<0x301, 0x00030e00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21158542598038] = TemplatedLoader<0, 0x00030e00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 2724130
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082686170853] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x00040205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082686170853] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x00040205u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I8_s16_
// num_verts= 2676210
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20775221563192] = TemplatedLoader<0x301, 0x00020f00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20775221563192] = TemplatedLoader<0, 0x00020f00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_
// num_verts= 2607642
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855299301] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855299301] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_s16_
// num_verts= 2514414
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30975986765797] = TemplatedLoader<0x301, 0x003f0f00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30975986765797] = TemplatedLoader<0, 0x003f0f00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_
// num_verts= 2351972
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20864995642168] = TemplatedLoader<0x301, 0x00030f00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20864995642168] = TemplatedLoader<0, 0x00030f00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_
// num_verts= 2097771
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30975967368515] = TemplatedLoader<0x301, 0x003f0f00u, 0x40a00c07u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30975967368515] = TemplatedLoader<0, 0x003f0f00u, 0x40a00c07u, 0x00000a05u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_
// num_verts= 1967616
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21066135453590] = TemplatedLoader<0x301, 0x00020a00u, 0x40e01007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21066135453590] = TemplatedLoader<0, 0x00020a00u, 0x40e01007u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_s16_
// num_verts= 1842624
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30647391726565] = TemplatedLoader<0x301, 0x003e0b00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30647391726565] = TemplatedLoader<0, 0x003e0b00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u8_
// num_verts= 1820688
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30975986344933] = TemplatedLoader<0x301, 0x003f0f00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30975986344933] = TemplatedLoader<0, 0x003f0f00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_
// num_verts= 1809504
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693865568406] = TemplatedLoader<0x301, 0x00020b00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693865568406] = TemplatedLoader<0, 0x00020b00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_
// num_verts= 1669848
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20772588497720] = TemplatedLoader<0x301, 0x00020b00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20772588497720] = TemplatedLoader<0, 0x00020b00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_
// num_verts= 1553110
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693207302038] = TemplatedLoader<0x301, 0x00020a00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693207302038] = TemplatedLoader<0, 0x00020a00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I8_u16_
// num_verts= 1481793
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20696479414422] = TemplatedLoader<0x301, 0x00020f00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20696479414422] = TemplatedLoader<0, 0x00020f00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_T3_mtx0_1_I16_u16_
// num_verts= 1464684
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63409997605861] = TemplatedLoader<0x301, 0x00ff0f00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63409997605861] = TemplatedLoader<0, 0x00ff0f00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_
// num_verts= 1353696
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21883891078373] = TemplatedLoader<0x301, 0x000a0a00u, 0x40201009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21883891078373] = TemplatedLoader<0, 0x000a0a00u, 0x40201009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 1329712
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082901653221] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x001c0205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082901653221] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x001c0205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 1322776
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49925482884837] = TemplatedLoader<0x301, 0x00af0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49925482884837] = TemplatedLoader<0, 0x00af0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_s16_
// num_verts= 1267708
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21066116234134] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21066116234134] = TemplatedLoader<0, 0x00020a00u, 0x40e00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u16_
// num_verts= 1220190
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30973353560037] = TemplatedLoader<0x301, 0x003f0b00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30973353560037] = TemplatedLoader<0, 0x003f0b00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I16_u16_
// num_verts= 1212106
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21155909532566] = TemplatedLoader<0x301, 0x00030a00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21155909532566] = TemplatedLoader<0, 0x00030a00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 1188024
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49756894867173] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49756894867173] = TemplatedLoader<0, 0x00ae0a00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 1161664
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49678171975397] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49678171975397] = TemplatedLoader<0, 0x00ae0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_
// num_verts= 1126080
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22715401622757] = TemplatedLoader<0x301, 0x000e0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22715401622757] = TemplatedLoader<0, 0x000e0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 1125099
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49172623404773] = TemplatedLoader<0x301, 0x00ab0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49172623404773] = TemplatedLoader<0, 0x00ab0a00u, 0x40a01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I16_u8_
// num_verts= 1118736
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30804837089253] = TemplatedLoader<0x301, 0x003e0b00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30804837089253] = TemplatedLoader<0, 0x003e0b00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I8_u16_
// num_verts= 1074759
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22209853052133] = TemplatedLoader<0x301, 0x000b0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22209853052133] = TemplatedLoader<0, 0x000b0a00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 937664
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49756966694629] = TemplatedLoader<0x301, 0x00ae0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49756966694629] = TemplatedLoader<0, 0x00ae0a00u, 0x40e01009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u8_
// num_verts= 898776
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49003963559653] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00040a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49003963559653] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00040a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_
// num_verts= 855504
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855018725] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855018725] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u8_
// num_verts= 844536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28107919779557] = TemplatedLoader<0x301, 0x002e0a00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28107919779557] = TemplatedLoader<0, 0x002e0a00u, 0x40a01009u, 0x00000205u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I8_u16_
// num_verts= 810616
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20990684674198] = TemplatedLoader<0x301, 0x00020f00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20990684674198] = TemplatedLoader<0, 0x00020f00u, 0x40a00c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_s16_
// num_verts= 767760
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27950474416869] = TemplatedLoader<0x301, 0x002e0a00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27950474416869] = TemplatedLoader<0, 0x002e0a00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_s16_
// num_verts= 737598
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28276436390629] = TemplatedLoader<0x301, 0x002f0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28276436390629] = TemplatedLoader<0, 0x002f0a00u, 0x40a01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_
// num_verts= 715662
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28107920060133] = TemplatedLoader<0x301, 0x002e0a00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28107920060133] = TemplatedLoader<0, 0x002e0a00u, 0x40a01009u, 0x00000a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 714480
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49928044122853] = TemplatedLoader<0x301, 0x00af0e00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49928044122853] = TemplatedLoader<0, 0x00af0e00u, 0x40e01009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I8_u8_
// num_verts= 702240
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20833238890646] = TemplatedLoader<0x301, 0x00020f00u, 0x40200c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20833238890646] = TemplatedLoader<0, 0x00020f00u, 0x40200c07u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_T2_mtx0_1_I8_s16_
// num_verts= 690984
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28186643092197] = TemplatedLoader<0x301, 0x002e0a00u, 0x40e01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28186643092197] = TemplatedLoader<0, 0x002e0a00u, 0x40e01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_
// num_verts= 661416
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22965273770213] = TemplatedLoader<0x301, 0x000f0e00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22965273770213] = TemplatedLoader<0, 0x000f0e00u, 0x40e01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u8_
// num_verts= 633876
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20707569040534] = TemplatedLoader<0x301, 0x00030f00u, 0x40201007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20707569040534] = TemplatedLoader<0, 0x00030f00u, 0x40201007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_flt_T0_mtx0_1_I16_u16_
// num_verts= 603273
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20865014824086] = TemplatedLoader<0x301, 0x00030f00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20865014824086] = TemplatedLoader<0, 0x00030f00u, 0x40a01007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_s16_
// num_verts= 578562
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855439041] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000e01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855439041] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000e01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_
// num_verts= 573078
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27276409515749] = TemplatedLoader<0x301, 0x002a0a00u, 0x40201009u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27276409515749] = TemplatedLoader<0, 0x002a0a00u, 0x40201009u, 0x00000a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_
// num_verts= 499044
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27512578331365] = TemplatedLoader<0x301, 0x002a0a00u, 0x40e01009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27512578331365] = TemplatedLoader<0, 0x002a0a00u, 0x40e01009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_u16_
// num_verts= 428352
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22718034688229] = TemplatedLoader<0x301, 0x000e0e00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22718034688229] = TemplatedLoader<0, 0x000e0e00u, 0x40a01009u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx1_3_I8_u16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_
// num_verts= 426874
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20987393304820] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00c05u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20987393304820] = TemplatedLoader<0, 0x00020a00u, 0x40a00c05u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_
// num_verts= 425010
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855298753] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855298753] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000a01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_
// num_verts= 404032
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22041336861377] = TemplatedLoader<0x301, 0x000a0a00u, 0x40a01009u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22041336861377] = TemplatedLoader<0, 0x000a0a00u, 0x40a01009u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_
// num_verts= 389364
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27276409656037] = TemplatedLoader<0x301, 0x002a0a00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27276409656037] = TemplatedLoader<0, 0x002a0a00u, 0x40201009u, 0x00000e05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u8_
// num_verts= 371964
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535742299030] = TemplatedLoader<0x301, 0x00020a00u, 0x40200c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535742299030] = TemplatedLoader<0, 0x00020a00u, 0x40200c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 351548
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004178760897] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x001c0201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004178760897] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x001c0201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_u16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_
// num_verts= 351360
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693207264500] = TemplatedLoader<0x301, 0x00020a00u, 0x40a01005u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693207264500] = TemplatedLoader<0, 0x00020a00u, 0x40a01005u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I8_s16_
// num_verts= 339416
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20774544039830] = TemplatedLoader<0x301, 0x00020e00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20774544039830] = TemplatedLoader<0, 0x00020e00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_
// num_verts= 326634
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535761556024] = TemplatedLoader<0x301, 0x00020a00u, 0x40201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535761556024] = TemplatedLoader<0, 0x00020a00u, 0x40201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 322322
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846733258469] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x001c0a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846733258469] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x001c0a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_u16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_
// num_verts= 293860
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693188045044] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00c05u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693188045044] = TemplatedLoader<0, 0x00020a00u, 0x40a00c05u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_T1_mtx1_1_I8_u16_T2_mtx0_1_I8_u16_
// num_verts= 289810
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27728046444789] = TemplatedLoader<0x301, 0x002a0a02u, 0x40a00c07u, 0x80000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27728046444789] = TemplatedLoader<0, 0x002a0a02u, 0x40a00c07u, 0x80000a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_s16_
// num_verts= 279684
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27276409655489] = TemplatedLoader<0x301, 0x002a0a00u, 0x40201009u, 0x00000e01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27276409655489] = TemplatedLoader<0, 0x002a0a00u, 0x40201009u, 0x00000e01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 275748
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846661431013] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846661431013] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00140a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 261983
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004179041473] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x001c0a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004179041473] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x001c0a01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_
// num_verts= 213616
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771930193814] = TemplatedLoader<0x301, 0x00020a00u, 0x40e01007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771930193814] = TemplatedLoader<0, 0x00020a00u, 0x40e01007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_
// num_verts= 206724
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20987398486297] = TemplatedLoader<0x301, 0x00020a02u, 0x40a00c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20987398486297] = TemplatedLoader<0, 0x00020a02u, 0x40a00c07u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_
// num_verts= 189736
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21883891077825] = TemplatedLoader<0x301, 0x000a0a00u, 0x40201009u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21883891077825] = TemplatedLoader<0, 0x000a0a00u, 0x40201009u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u8_
// num_verts= 181224
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22120059753153] = TemplatedLoader<0x301, 0x000a0a00u, 0x40e01009u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22120059753153] = TemplatedLoader<0, 0x000a0a00u, 0x40e01009u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_
// num_verts= 170004
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27433855018177] = TemplatedLoader<0x301, 0x002a0a00u, 0x40a01009u, 0x00000201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27433855018177] = TemplatedLoader<0, 0x002a0a00u, 0x40a01009u, 0x00000201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 137836
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49003963278529] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00040201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49003963278529] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00040201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 108836
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846732977893] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x001c0205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846732977893] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x001c0205u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_
// num_verts= 90048
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20864995604630] = TemplatedLoader<0x301, 0x00030f00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20864995604630] = TemplatedLoader<0, 0x00030f00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 87906
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846517495525] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00040205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846517495525] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00040205u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_T2_mtx0_1_Dir_flt_
// num_verts= 77936
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[24058683649720] = TemplatedLoader<0x301, 0x00151100u, 0x41216008u, 0x00001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[24058683649720] = TemplatedLoader<0, 0x00151100u, 0x41216008u, 0x00001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u16_
// num_verts= 59072
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082829825765] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x00140205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082829825765] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x00140205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u8_
// num_verts= 52212
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082686451429] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x00040a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082686451429] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x00040a05u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_u16_C0_1_Dir_8888_
// num_verts= 48344
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20165589953524] = TemplatedLoader<0x301, 0x00001100u, 0x40016005u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20165589953524] = TemplatedLoader<0, 0x00001100u, 0x40016005u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_u16_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_
// num_verts= 43920
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535761480948] = TemplatedLoader<0x301, 0x00020a00u, 0x40201005u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535761480948] = TemplatedLoader<0, 0x00020a00u, 0x40201005u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u8_
// num_verts= 29302
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846517776101] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00040a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846517776101] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00040a05u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_
// num_verts= 19364
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846661430465] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00140a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846661430465] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00140a01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_
// num_verts= 16452
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27276409515201] = TemplatedLoader<0x301, 0x002a0a00u, 0x40201009u, 0x00000a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27276409515201] = TemplatedLoader<0, 0x002a0a00u, 0x40201009u, 0x00000a01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u16_
// num_verts= 14768
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49082829825217] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40e01009u, 0x00140201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49082829825217] = TemplatedLoader<0, 0x00aa0a00u, 0x40e01009u, 0x00140201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u16_
// num_verts= 12162
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846661150437] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00140205u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846661150437] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00140205u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_s16_
// num_verts= 8372
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846733257921] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x001c0a01u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846733257921] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x001c0a01u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u16_
// num_verts= 8108
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49004106933441] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40a01009u, 0x00140201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49004106933441] = TemplatedLoader<0, 0x00aa0a00u, 0x40a01009u, 0x00140201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_u8_
// num_verts= 8108
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846517494977] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x00040201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846517494977] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x00040201u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_T2_mtx0_1_I8_u8_T3_mtx0_1_I8_s16_
// num_verts= 8108
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[48846732977345] = TemplatedLoader<0x301, 0x00aa0a00u, 0x40201009u, 0x001c0201u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[48846732977345] = TemplatedLoader<0, 0x00aa0a00u, 0x40201009u, 0x001c0201u, 0x00000000u>;
	}
}
