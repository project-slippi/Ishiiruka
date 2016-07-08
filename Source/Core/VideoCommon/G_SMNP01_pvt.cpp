// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_SMNP01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_SMNP01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_T3_mtx1_1_Inv_flt_
// num_verts= 18309112
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21261039447109] = TemplatedLoader<0x301, 0x00032f0eu, 0x40e16c07u, 0x80241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21261039447109] = TemplatedLoader<0, 0x00032f0eu, 0x40e16c07u, 0x80241209u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_
// num_verts= 14050512
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20963841266838] = TemplatedLoader<0x301, 0x00032b00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20963841266838] = TemplatedLoader<0, 0x00032b00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 9359520
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21181972760957] = TemplatedLoader<0x301, 0x00032f06u, 0x40a16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21181972760957] = TemplatedLoader<0, 0x00032f06u, 0x40a16c07u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 7374770
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260695652733] = TemplatedLoader<0x301, 0x00032f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260695652733] = TemplatedLoader<0, 0x00032f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_T3_mtx1_1_Inv_flt_
// num_verts= 4908918
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21182316555333] = TemplatedLoader<0x301, 0x00032f0eu, 0x40a16c07u, 0x80241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21182316555333] = TemplatedLoader<0, 0x00032f0eu, 0x40a16c07u, 0x80241209u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 3076962
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21159216962591] = TemplatedLoader<0x301, 0x00030f06u, 0x40a00c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21159216962591] = TemplatedLoader<0, 0x00030f06u, 0x40a00c09u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_T0_mtx0_1_I8_u16_
// num_verts= 2801136
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20687864293270] = TemplatedLoader<0x301, 0x00020200u, 0x40a00007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20687864293270] = TemplatedLoader<0, 0x00020200u, 0x40a00007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_
// num_verts= 2743160
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20716602222392] = TemplatedLoader<0x301, 0x00022b00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20716602222392] = TemplatedLoader<0, 0x00022b00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_
// num_verts= 2333156
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20966474332310] = TemplatedLoader<0x301, 0x00032f00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20966474332310] = TemplatedLoader<0, 0x00032f00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_u8_T0_mtx0_1_Dir_flt_
// num_verts= 1984592
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676793755231] = TemplatedLoader<0x301, 0x00010200u, 0x41200000u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676793755231] = TemplatedLoader<0, 0x00010200u, 0x41200000u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 1828008
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21092179462525] = TemplatedLoader<0x301, 0x00022f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21092179462525] = TemplatedLoader<0, 0x00022f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_
// num_verts= 1149256
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20797299913272] = TemplatedLoader<0x301, 0x00022e00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20797299913272] = TemplatedLoader<0, 0x00022e00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_
// num_verts= 751756
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20795325076630] = TemplatedLoader<0x301, 0x00022b00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20795325076630] = TemplatedLoader<0, 0x00022b00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_s16_
// num_verts= 605280
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20795286675256] = TemplatedLoader<0x301, 0x00022b00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20795286675256] = TemplatedLoader<0, 0x00022b00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_T3_mtx1_1_Inv_flt_
// num_verts= 589400
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21235650583271] = TemplatedLoader<0x301, 0x00030b0eu, 0x40e00c09u, 0x80241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21235650583271] = TemplatedLoader<0, 0x00030b0eu, 0x40e00c09u, 0x80241209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_
// num_verts= 533280
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20710620129174] = TemplatedLoader<0x301, 0x00022200u, 0x40a16007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20710620129174] = TemplatedLoader<0, 0x00022200u, 0x40a16007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u8_
// num_verts= 497840
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20553174345622] = TemplatedLoader<0x301, 0x00022200u, 0x40216007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20553174345622] = TemplatedLoader<0, 0x00022200u, 0x40216007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
// num_verts= 413216
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22137472543573] = TemplatedLoader<0x301, 0x000a2200u, 0x40e16007u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22137472543573] = TemplatedLoader<0, 0x000a2200u, 0x40e16007u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_
// num_verts= 406624
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20718577021496] = TemplatedLoader<0x301, 0x00022e00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20718577021496] = TemplatedLoader<0, 0x00022e00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 259336
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088888130685] = TemplatedLoader<0x301, 0x00022a06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088888130685] = TemplatedLoader<0, 0x00022a06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_
// num_verts= 207968
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22064073478647] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a16c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22064073478647] = TemplatedLoader<0, 0x000a2a00u, 0x40a16c09u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_flt_T0_mtx0_1_Dir_u8_
// num_verts= 196228
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20361244071911] = TemplatedLoader<0x301, 0x00010100u, 0x40200008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20361244071911] = TemplatedLoader<0, 0x00010100u, 0x40200008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_
// num_verts= 147024
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142757893699] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e16407u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142757893699] = TemplatedLoader<0, 0x000a2a00u, 0x40e16407u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_
// num_verts= 91568
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22064073478373] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a16c09u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22064073478373] = TemplatedLoader<0, 0x000a2a00u, 0x40a16c09u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_
// num_verts= 88880
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20789343002181] = TemplatedLoader<0x301, 0x00022200u, 0x40e16006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20789343002181] = TemplatedLoader<0, 0x00022200u, 0x40e16006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s8_
// num_verts= 66660
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20631897237398] = TemplatedLoader<0x301, 0x00022200u, 0x40616007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20631897237398] = TemplatedLoader<0, 0x00022200u, 0x40616007u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u8_
// num_verts= 49664
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535703897656] = TemplatedLoader<0x301, 0x00020a00u, 0x40200409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535703897656] = TemplatedLoader<0, 0x00020a00u, 0x40200409u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s8_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_
// num_verts= 42240
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771872460370] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00403u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771872460370] = TemplatedLoader<0, 0x00020a00u, 0x40e00403u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_
// num_verts= 26384
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22064035039461] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a16409u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22064035039461] = TemplatedLoader<0, 0x000a2a00u, 0x40a16409u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx1_2_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 22935
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010126781228] = TemplatedLoader<0x301, 0x00022a06u, 0x40a16406u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010126781228] = TemplatedLoader<0, 0x00022a06u, 0x40a16406u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_
// num_verts= 21728
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22120040534245] = TemplatedLoader<0x301, 0x000a0a00u, 0x40e00c09u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22120040534245] = TemplatedLoader<0, 0x000a0a00u, 0x40e00c09u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_
// num_verts= 16296
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010126799997] = TemplatedLoader<0x301, 0x00022a06u, 0x40a16407u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010126799997] = TemplatedLoader<0, 0x00022a06u, 0x40a16407u, 0x80001209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_s8_
// num_verts= 14084
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20637182587798] = TemplatedLoader<0x301, 0x00022a00u, 0x40616407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20637182587798] = TemplatedLoader<0, 0x00022a00u, 0x40616407u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_
// num_verts= 12416
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22041279203557] = TemplatedLoader<0x301, 0x000a0a00u, 0x40a00409u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22041279203557] = TemplatedLoader<0, 0x000a0a00u, 0x40a00409u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u8_
// num_verts= 11640
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22064035038913] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a16409u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22064035038913] = TemplatedLoader<0, 0x000a2a00u, 0x40a16409u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s8_T0_mtx0_1_I8_u16_
// num_verts= 10192
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20687864218194] = TemplatedLoader<0x301, 0x00020200u, 0x40a00003u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20687864218194] = TemplatedLoader<0, 0x00020200u, 0x40a00003u, 0x00000000u, 0x00000000u>;
	}
}
