// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#include "VideoCommon/G_RMGP01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"


void G_RMGP01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_ num_verts= 1634718708
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20977006594198] = TemplatedLoader<0x301, 0x00033f00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20977006594198] = TemplatedLoader<0, 0x00033f00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_ num_verts= 282808496
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20162639804566] = TemplatedLoader<0x301, 0x00000f00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20162639804566] = TemplatedLoader<0, 0x00000f00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_ num_verts= 148684510
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271211853974] = TemplatedLoader<0x301, 0x00033f00u, 0x40e16c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271211853974] = TemplatedLoader<0, 0x00033f00u, 0x40e16c07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_s16_ num_verts= 123985257
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20943718496406] = TemplatedLoader<0x301, 0x00030f00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20943718496406] = TemplatedLoader<0, 0x00030f00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s8_T0_mtx0_1_I8_s8_ num_verts= 42056768
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20609141326418] = TemplatedLoader<0x301, 0x00020200u, 0x40600003u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20609141326418] = TemplatedLoader<0, 0x00020200u, 0x40600003u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_ num_verts= 33390180
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20456845064342] = TemplatedLoader<0x301, 0x00000f00u, 0x40000c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20456845064342] = TemplatedLoader<0, 0x00000f00u, 0x40000c07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_ num_verts= 20656335
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20977006631736] = TemplatedLoader<0x301, 0x00033f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20977006631736] = TemplatedLoader<0, 0x00033f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_ num_verts= 17966340
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20969049739414] = TemplatedLoader<0x301, 0x00033300u, 0x40e16007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20969049739414] = TemplatedLoader<0, 0x00033300u, 0x40e16007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_T0_mtx0_1_I16_s16_ num_verts= 16858875
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20943718533944] = TemplatedLoader<0x301, 0x00030f00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20943718533944] = TemplatedLoader<0, 0x00030f00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_s16_ num_verts= 15811590
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20969049776952] = TemplatedLoader<0x301, 0x00033300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20969049776952] = TemplatedLoader<0, 0x00033300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_ num_verts= 14616395
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271216997913] = TemplatedLoader<0x301, 0x00033f02u, 0x40e16c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271216997913] = TemplatedLoader<0, 0x00033f02u, 0x40e16c07u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_Dir_flt_ num_verts= 12531821
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676135657784] = TemplatedLoader<0x301, 0x00010100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676135657784] = TemplatedLoader<0, 0x00010100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx1_1_I16_s16_ num_verts= 10647072
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271214425327] = TemplatedLoader<0x301, 0x00033f01u, 0x40e16c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271214425327] = TemplatedLoader<0, 0x00033f01u, 0x40e16c07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_ num_verts= 10222171
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20195927902358] = TemplatedLoader<0x301, 0x00003f00u, 0x40016c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20195927902358] = TemplatedLoader<0, 0x00003f00u, 0x40016c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_Nrm_0_0_Dir_flt_ num_verts= 6757672
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20156076397880] = TemplatedLoader<0x301, 0x00000500u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20156076397880] = TemplatedLoader<0, 0x00000500u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 6757672
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21350200419849] = TemplatedLoader<0x301, 0x00050100u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21350200419849] = TemplatedLoader<0, 0x00050100u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_ num_verts= 5560606
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20153366454584] = TemplatedLoader<0x301, 0x00000100u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20153366454584] = TemplatedLoader<0, 0x00000100u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 5353320
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271227914621] = TemplatedLoader<0x301, 0x00033f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271227914621] = TemplatedLoader<0, 0x00033f06u, 0x40e16c07u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_T2_mtx0_1_I16_flt_T3_mtx0_1_I16_flt_ num_verts= 4578000
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63491795400713] = TemplatedLoader<0x301, 0x00ff1100u, 0x41216009u, 0x00241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63491795400713] = TemplatedLoader<0, 0x00ff1100u, 0x41216009u, 0x00241209u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx1_1_I16_s16_T1_mtx1_1_Inv_flt_ num_verts= 3691776
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271219569266] = TemplatedLoader<0x301, 0x00033f03u, 0x40e16c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271219569266] = TemplatedLoader<0, 0x00033f03u, 0x40e16c07u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_u16_ num_verts= 3349472
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20530913448248] = TemplatedLoader<0x301, 0x00011100u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20530913448248] = TemplatedLoader<0, 0x00011100u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_s16_ num_verts= 2872184
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21237923756182] = TemplatedLoader<0x301, 0x00030f00u, 0x40e00c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21237923756182] = TemplatedLoader<0, 0x00030f00u, 0x40e00c07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_T0_mtx0_1_Dir_flt_ num_verts= 2805260
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676135639015] = TemplatedLoader<0x301, 0x00010100u, 0x41200008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676135639015] = TemplatedLoader<0, 0x00010100u, 0x41200008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_ num_verts= 2330456
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20490133162134] = TemplatedLoader<0x301, 0x00003f00u, 0x40016c07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20490133162134] = TemplatedLoader<0, 0x00003f00u, 0x40016c07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_ num_verts= 2286516
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20165590028600] = TemplatedLoader<0x301, 0x00001100u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20165590028600] = TemplatedLoader<0, 0x00001100u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
	// num_verts= 2883600
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23047585895945] = TemplatedLoader<0x301, 0x000f1100u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23047585895945] = TemplatedLoader<0, 0x000f1100u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T2_mtx1_1_Inv_flt_ num_verts= 1255222
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271222770682] = TemplatedLoader<0x301, 0x00033f04u, 0x40e16c07u, 0x80001200u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271222770682] = TemplatedLoader<0, 0x00033f04u, 0x40e16c07u, 0x80001200u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_I8_flt_ num_verts= 1003520
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20844651847992] = TemplatedLoader<0x301, 0x00020100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20844651847992] = TemplatedLoader<0, 0x00020100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_ num_verts= 866912
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20688359213031] = TemplatedLoader<0x301, 0x00011100u, 0x41216008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20688359213031] = TemplatedLoader<0, 0x00011100u, 0x41216008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_u16_T0_mtx0_1_Dir_u8_ num_verts= 203608
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20361243996835] = TemplatedLoader<0x301, 0x00010100u, 0x40200004u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20361243996835] = TemplatedLoader<0, 0x00010100u, 0x40200004u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_u16_T0_mtx0_1_Dir_flt_ num_verts= 95756
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676135563939] = TemplatedLoader<0x301, 0x00010100u, 0x41200004u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676135563939] = TemplatedLoader<0, 0x00010100u, 0x41200004u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_ num_verts= 87756
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20162639842104] = TemplatedLoader<0x301, 0x00000f00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20162639842104] = TemplatedLoader<0, 0x00000f00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_ num_verts= 45540
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20688359231800] = TemplatedLoader<0x301, 0x00011100u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20688359231800] = TemplatedLoader<0, 0x00011100u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_ num_verts= 43488
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21237928900121] = TemplatedLoader<0x301, 0x00030f02u, 0x40e00c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21237928900121] = TemplatedLoader<0, 0x00030f02u, 0x40e00c07u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_u16_ num_verts= 24152
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20153366360739] = TemplatedLoader<0x301, 0x00000100u, 0x40000004u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20153366360739] = TemplatedLoader<0, 0x00000100u, 0x40000004u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_C0_1_Dir_8888_ num_verts= 6992
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20165590009831] = TemplatedLoader<0x301, 0x00001100u, 0x40016008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20165590009831] = TemplatedLoader<0, 0x00001100u, 0x40016008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_s16_ num_verts= 2520
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20609636340024] = TemplatedLoader<0x301, 0x00011100u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20609636340024] = TemplatedLoader<0, 0x00011100u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 1852
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22999200877653] = TemplatedLoader<0x301, 0x000f3f00u, 0x40e16c07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22999200877653] = TemplatedLoader<0, 0x000f3f00u, 0x40e16c07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_u16_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 1064
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21350200326004] = TemplatedLoader<0x301, 0x00050100u, 0x41200004u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21350200326004] = TemplatedLoader<0, 0x00050100u, 0x41200004u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_ num_verts= 924
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20153366435815] = TemplatedLoader<0x301, 0x00000100u, 0x40000008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20153366435815] = TemplatedLoader<0, 0x00000100u, 0x40000008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_s16_ num_verts= 768
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20609636321255] = TemplatedLoader<0x301, 0x00011100u, 0x40e16008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20609636321255] = TemplatedLoader<0, 0x00011100u, 0x40e16008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 28
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21362423975096] = TemplatedLoader<0x301, 0x00051100u, 0x41216008u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21362423975096] = TemplatedLoader<0, 0x00051100u, 0x41216008u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 17684060
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22999200915191] = TemplatedLoader<0x301, 0x000f3f00u, 0x40e16c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22999200915191] = TemplatedLoader<0, 0x000f3f00u, 0x40e16c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_C0_1_I16_8888_ num_verts= 2941856
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20187971047574] = TemplatedLoader<0x301, 0x00003300u, 0x40016007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20187971047574] = TemplatedLoader<0, 0x00003300u, 0x40016007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_8888_ num_verts= 738660
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20195927939896] = TemplatedLoader<0x301, 0x00003f00u, 0x40016c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20195927939896] = TemplatedLoader<0, 0x00003f00u, 0x40016c09u, 0x00000000u, 0x00000000u>;
	}
}
