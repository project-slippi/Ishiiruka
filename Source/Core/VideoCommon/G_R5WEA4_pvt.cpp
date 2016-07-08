// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#include "VideoCommon/G_R5WEA4_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_R5WEA4_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_flt_
// num_verts= 930221933
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21022460645176] = TemplatedLoader<0x301, 0x00030f00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21022460645176] = TemplatedLoader<0, 0x00030f00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_flt_
// num_verts= 170744505
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21055748742968] = TemplatedLoader<0x301, 0x00033f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21055748742968] = TemplatedLoader<0, 0x00033f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_
// num_verts= 99643871
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21045216481080] = TemplatedLoader<0x301, 0x00032f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21045216481080] = TemplatedLoader<0, 0x00032f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
// num_verts= 82861554
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23067410764809] = TemplatedLoader<0x301, 0x000f2f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23067410764809] = TemplatedLoader<0, 0x000f2f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I8_flt_
// num_verts= 70775672
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20853944454968] = TemplatedLoader<0x301, 0x00020f00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20853944454968] = TemplatedLoader<0, 0x00020f00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_
// num_verts= 44375931
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21042583415608] = TemplatedLoader<0x301, 0x00032b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21042583415608] = TemplatedLoader<0, 0x00032b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I16_8888_T0_mtx0_1_I16_flt_
// num_verts= 35965818
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21053115677496] = TemplatedLoader<0x301, 0x00033b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21053115677496] = TemplatedLoader<0, 0x00033b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I8_flt_
// num_verts= 30598273
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22393346003977] = TemplatedLoader<0x301, 0x000b2f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22393346003977] = TemplatedLoader<0, 0x000b2f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 23208347
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21144858382904] = TemplatedLoader<0x301, 0x00020a00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21144858382904] = TemplatedLoader<0, 0x00020a00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 22619394
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20867407683896] = TemplatedLoader<0x301, 0x00022100u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20867407683896] = TemplatedLoader<0, 0x00022100u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
// num_verts= 21681152
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23064777699337] = TemplatedLoader<0x301, 0x000f2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23064777699337] = TemplatedLoader<0, 0x000f2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 14145825
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20851311389496] = TemplatedLoader<0x301, 0x00020b00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20851311389496] = TemplatedLoader<0, 0x00020b00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I16_flt_
// num_verts= 8667078
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21019169313336] = TemplatedLoader<0x301, 0x00030a00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21019169313336] = TemplatedLoader<0, 0x00030a00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 5439778
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20874067225400] = TemplatedLoader<0x301, 0x00022b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20874067225400] = TemplatedLoader<0, 0x00022b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I16_flt_
// num_verts= 3898465
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22896261509129] = TemplatedLoader<0x301, 0x000e2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22896261509129] = TemplatedLoader<0, 0x000e2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s16_C0_1_Dir_8888_T0_mtx0_1_Dir_s16_
// num_verts= 3676650
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20609636283717] = TemplatedLoader<0x301, 0x00011100u, 0x40e16006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20609636283717] = TemplatedLoader<0, 0x00011100u, 0x40e16006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I8_flt_
// num_verts= 2963168
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22403878265865] = TemplatedLoader<0x301, 0x000b3f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22403878265865] = TemplatedLoader<0, 0x000b3f00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 2500323
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20876700290872] = TemplatedLoader<0x301, 0x00022f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20876700290872] = TemplatedLoader<0, 0x00022f00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 2046291
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22222196748297] = TemplatedLoader<0x301, 0x000a2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22222196748297] = TemplatedLoader<0, 0x000a2b00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I16_8888_T0_mtx0_1_I8_flt_
// num_verts= 1803242
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20884599487288] = TemplatedLoader<0x301, 0x00023b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20884599487288] = TemplatedLoader<0, 0x00023b00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s16_T0_mtx0_1_Dir_s16_
// num_verts= 956800
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20597412709701] = TemplatedLoader<0x301, 0x00010100u, 0x40e00006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20597412709701] = TemplatedLoader<0, 0x00010100u, 0x40e00006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_Nrm_0_0_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_
// num_verts= 523901
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20691069175096] = TemplatedLoader<0x301, 0x00011500u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20691069175096] = TemplatedLoader<0, 0x00011500u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_Nrm_0_0_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_
// num_verts= 236338
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21365133937161] = TemplatedLoader<0x301, 0x00051500u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21365133937161] = TemplatedLoader<0, 0x00051500u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_
// num_verts= 148131
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21339421740856] = TemplatedLoader<0x301, 0x00032f00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21339421740856] = TemplatedLoader<0, 0x00032f00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s16_C0_1_Dir_8888_T0_mtx0_1_Dir_s16_T1_mtx0_1_Dir_s16_
// num_verts= 78708
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21283701045508] = TemplatedLoader<0x301, 0x00051100u, 0x40e16006u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21283701045508] = TemplatedLoader<0, 0x00051100u, 0x40e16006u, 0x00000007u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_Nrm_0_0_Dir_flt_T0_mtx0_1_Dir_flt_
// num_verts= 5601
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20678845601080] = TemplatedLoader<0x301, 0x00010500u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20678845601080] = TemplatedLoader<0, 0x00010500u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 3706
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21167614218808] = TemplatedLoader<0x301, 0x00022a00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21167614218808] = TemplatedLoader<0, 0x00022a00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s16_T0_mtx0_1_Dir_s16_T1_mtx0_1_Dir_s16_T2_mtx0_1_Dir_s16_T3_mtx0_1_Dir_s16_T4_mtx0_1_Dir_s16_T5_mtx0_1_Dir_s16_
// num_verts= 2028
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[250582462842852] = TemplatedLoader<0x301, 0x05550100u, 0x40e00006u, 0x381c0e07u, 0x000000e0u>;
	}
	else
#endif
	{
		pvlmap[250582462842852] = TemplatedLoader<0, 0x05550100u, 0x40e00006u, 0x381c0e07u, 0x000000e0u>;
	}
}
