// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#include "VideoCommon/G_SX4E01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"


void G_SX4E01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I16_8888_T0_mtx0_1_I16_flt_ num_verts= 603419813
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21055575767864] = TemplatedLoader<0x301, 0x00033f00u, 0x41214c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21055575767864] = TemplatedLoader<0, 0x00033f00u, 0x41214c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_6666_T0_mtx0_1_I16_s16_ num_verts= 410579904
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20976699120440] = TemplatedLoader<0x301, 0x00033f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20976699120440] = TemplatedLoader<0, 0x00033f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_ num_verts= 373033578
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20966166858552] = TemplatedLoader<0x301, 0x00032f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20966166858552] = TemplatedLoader<0, 0x00032f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_ num_verts= 323416264
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260064569494] = TemplatedLoader<0x301, 0x00032f00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260064569494] = TemplatedLoader<0, 0x00032f00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 322634314
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20873235983928] = TemplatedLoader<0x301, 0x00022a00u, 0x41214c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20873235983928] = TemplatedLoader<0, 0x00022a00u, 0x41214c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_ num_verts= 264981920
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20963533793080] = TemplatedLoader<0x301, 0x00032b00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20963533793080] = TemplatedLoader<0, 0x00032b00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I16_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_ num_verts= 242554094
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23077770051593] = TemplatedLoader<0x301, 0x000f3f00u, 0x41214c09u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23077770051593] = TemplatedLoader<0, 0x000f3f00u, 0x41214c09u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_ num_verts= 183695088
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31087671024887] = TemplatedLoader<0x301, 0x003f3f00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31087671024887] = TemplatedLoader<0, 0x003f3f00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I16_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_T3_mtx0_1_I16_s16_ num_verts= 155397872
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63443030940919] = TemplatedLoader<0x301, 0x00ff3f00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63443030940919] = TemplatedLoader<0, 0x00ff3f00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_ num_verts= 143795566
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21181341677718] = TemplatedLoader<0x301, 0x00032f00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21181341677718] = TemplatedLoader<0, 0x00032f00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 122446640
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794359336504] = TemplatedLoader<0x301, 0x00022a00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794359336504] = TemplatedLoader<0, 0x00022a00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I16_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_T2_mtx0_1_I16_flt_ num_verts= 79747928
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31166547812873] = TemplatedLoader<0x301, 0x003f3f00u, 0x41214c09u, 0x00001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31166547812873] = TemplatedLoader<0, 0x003f3f00u, 0x41214c09u, 0x00001209u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 62441350
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21009534193208] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21009534193208] = TemplatedLoader<0, 0x00022a00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_0_I8_6666_T0_mtx0_1_I8_flt_ num_verts= 61928416
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20867604683320] = TemplatedLoader<0x301, 0x00022200u, 0x41210009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20867604683320] = TemplatedLoader<0, 0x00022200u, 0x41210009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_T2_mtx0_1_I8_flt_ num_verts= 58929704
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27613884224777] = TemplatedLoader<0x301, 0x002a2a00u, 0x41214c09u, 0x00001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27613884224777] = TemplatedLoader<0, 0x002a2a00u, 0x41214c09u, 0x00001209u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 57961635
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22357663715557] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22357663715557] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_ num_verts= 57474608
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20965859309718] = TemplatedLoader<0x301, 0x00032f00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20965859309718] = TemplatedLoader<0, 0x00032f00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 46600266
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794051787670] = TemplatedLoader<0x301, 0x00022a00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794051787670] = TemplatedLoader<0, 0x00022a00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 41865114
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21009534155670] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21009534155670] = TemplatedLoader<0, 0x00022a00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_T3_mtx0_1_I16_s16_ num_verts= 32419807
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63432498679031] = TemplatedLoader<0x301, 0x00ff2f00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63432498679031] = TemplatedLoader<0, 0x00ff2f00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 32217410
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715328895894] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715328895894] = TemplatedLoader<0, 0x00022a00u, 0x40a0ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 28876128
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23282258890487] = TemplatedLoader<0x301, 0x000f2f00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23282258890487] = TemplatedLoader<0, 0x000f2f00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_u16_ num_verts= 23565542
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23203535998437] = TemplatedLoader<0x301, 0x000f2f00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23203535998437] = TemplatedLoader<0, 0x000f2f00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 22818509
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20795017602872] = TemplatedLoader<0x301, 0x00022b00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20795017602872] = TemplatedLoader<0, 0x00022b00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 22459048
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088257047446] = TemplatedLoader<0x301, 0x00022a00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088257047446] = TemplatedLoader<0, 0x00022a00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_0_Dir_565_T0_mtx0_1_I8_flt_ num_verts= 21468888
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20855184109880] = TemplatedLoader<0x301, 0x00021100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20855184109880] = TemplatedLoader<0, 0x00021100u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 21225964
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22357663678019] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec07u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22357663678019] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec07u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_ num_verts= 21146720
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260064607032] = TemplatedLoader<0x301, 0x00032f00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260064607032] = TemplatedLoader<0, 0x00032f00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_T3_mtx0_1_I16_s16_ num_verts= 20708198
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[63429865613559] = TemplatedLoader<0x301, 0x00ff2b00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[63429865613559] = TemplatedLoader<0, 0x00ff2b00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 20460426
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21012825487510] = TemplatedLoader<0x301, 0x00022f00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21012825487510] = TemplatedLoader<0, 0x00022f00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_ num_verts= 20263995
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27535007436791] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27535007436791] = TemplatedLoader<0, 0x002a2a00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 20041784
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20797650668344] = TemplatedLoader<0x301, 0x00022f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20797650668344] = TemplatedLoader<0, 0x00022f00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 17487197
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142488859127] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142488859127] = TemplatedLoader<0, 0x000a2a00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s8_T0_mtx0_1_Dir_flt_ num_verts= 16806764
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676793811538] = TemplatedLoader<0x301, 0x00010200u, 0x41200003u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676793811538] = TemplatedLoader<0, 0x00010200u, 0x41200003u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 15632744
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21091548416824] = TemplatedLoader<0x301, 0x00022f00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21091548416824] = TemplatedLoader<0, 0x00022f00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I8_s16_ num_verts= 14907928
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[28380879719671] = TemplatedLoader<0x301, 0x002f2f00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[28380879719671] = TemplatedLoader<0, 0x002f2f00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_6666_T0_mtx0_1_I8_u16_ num_verts= 14741856
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20710970921784] = TemplatedLoader<0x301, 0x00022300u, 0x40a12009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20710970921784] = TemplatedLoader<0, 0x00022300u, 0x40a12009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 14640912
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21012825525048] = TemplatedLoader<0x301, 0x00022f00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21012825525048] = TemplatedLoader<0, 0x00022f00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s8_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 14224940
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21350858573603] = TemplatedLoader<0x301, 0x00050200u, 0x41200003u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21350858573603] = TemplatedLoader<0, 0x00050200u, 0x41200003u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 14163744
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794979163960] = TemplatedLoader<0x301, 0x00022b00u, 0x40e12409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794979163960] = TemplatedLoader<0, 0x00022b00u, 0x40e12409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 13826709
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22436386607607] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22436386607607] = TemplatedLoader<0, 0x000a2a00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 13612123
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22988361142007] = TemplatedLoader<0x301, 0x000f2f00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22988361142007] = TemplatedLoader<0, 0x000f2f00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 13007414
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22985728076535] = TemplatedLoader<0x301, 0x000f2b00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22985728076535] = TemplatedLoader<0, 0x000f2b00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_ num_verts= 12374503
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21256773237654] = TemplatedLoader<0x301, 0x00032a00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21256773237654] = TemplatedLoader<0, 0x00032a00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 12329550
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063765929813] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a12c07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063765929813] = TemplatedLoader<0, 0x000a2a00u, 0x40a12c07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_s16_ num_verts= 12033840
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23203535961173] = TemplatedLoader<0x301, 0x000f2f00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23203535961173] = TemplatedLoader<0, 0x000f2f00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 11812984
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22439677939447] = TemplatedLoader<0x301, 0x000a2f00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22439677939447] = TemplatedLoader<0, 0x000a2f00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_ num_verts= 11466076
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715636407190] = TemplatedLoader<0x301, 0x00022a00u, 0x40a12c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715636407190] = TemplatedLoader<0, 0x00022a00u, 0x40a12c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 11376722
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20710005106582] = TemplatedLoader<0x301, 0x00022200u, 0x40a0e007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20710005106582] = TemplatedLoader<0, 0x00022200u, 0x40a0e007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_T1_mtx1_1_I16_u16_T2_mtx0_1_I16_s16_ num_verts= 11235316
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31292318724597] = TemplatedLoader<0x301, 0x003f2f02u, 0x40a0ec07u, 0x80000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31292318724597] = TemplatedLoader<0, 0x003f2f02u, 0x40a0ec07u, 0x80000e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 10744341
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794320897592] = TemplatedLoader<0x301, 0x00022a00u, 0x40e12409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794320897592] = TemplatedLoader<0, 0x00022a00u, 0x40e12409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_s16_ num_verts= 10070256
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49026608287735] = TemplatedLoader<0x301, 0x00aa2a00u, 0x40a12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49026608287735] = TemplatedLoader<0, 0x00aa2a00u, 0x40a12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_T3_mtx0_1_I16_s16_T4_mtx0_1_I16_s16_T5_mtx0_1_I16_s16_T6_mtx0_1_I16_s16_T7_mtx0_1_I16_s16_ num_verts= 10045574
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[11064216813136343] = TemplatedLoader<0x301, 0xffff2b00u, 0x40a12c09u, 0x381c0e07u, 0x0381c0e0u>;
	}
	else
#endif
	{
		pvlmap[11064216813136343] = TemplatedLoader<0, 0xffff2b00u, 0x40a12c09u, 0x381c0e07u, 0x0381c0e0u>;
	}
	//P_mtx0_3_Dir_flt_C0_0_I16_8888_T0_mtx0_1_I8_flt_ num_verts= 9751332
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20877786190136] = TemplatedLoader<0x301, 0x00023100u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20877786190136] = TemplatedLoader<0, 0x00023100u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_u16_ num_verts= 9721092
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715598005816] = TemplatedLoader<0x301, 0x00022a00u, 0x40a12409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715598005816] = TemplatedLoader<0, 0x00022a00u, 0x40a12409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 9630304
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142181347831] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e0ec09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142181347831] = TemplatedLoader<0, 0x000a2a00u, 0x40e0ec09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_ num_verts= 9157022
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30827266615543] = TemplatedLoader<0x301, 0x003e2b00u, 0x40a12c09u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30827266615543] = TemplatedLoader<0, 0x003e2b00u, 0x40a12c09u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_ num_verts= 8931366
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794359298966] = TemplatedLoader<0x301, 0x00022a00u, 0x40e12c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794359298966] = TemplatedLoader<0, 0x00022a00u, 0x40e12c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_s16_T4_mtx0_1_I8_s16_ num_verts= 8762110
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[135514335366885] = TemplatedLoader<0x301, 0x02aa2a00u, 0x40e12c09u, 0x381c0e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[135514335366885] = TemplatedLoader<0, 0x02aa2a00u, 0x40e12c09u, 0x381c0e05u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 8673440
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22360955047397] = TemplatedLoader<0x301, 0x000a2f00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22360955047397] = TemplatedLoader<0, 0x000a2f00u, 0x40a0ec09u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 8614018
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22145780190967] = TemplatedLoader<0x301, 0x000a2f00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22145780190967] = TemplatedLoader<0, 0x000a2f00u, 0x40e12c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_s16_ num_verts= 8576995
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49105331179511] = TemplatedLoader<0x301, 0x00aa2a00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49105331179511] = TemplatedLoader<0, 0x00aa2a00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 8045348
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715328933432] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0ec09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715328933432] = TemplatedLoader<0, 0x00022a00u, 0x40a0ec09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_ num_verts= 7875270
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27535007399253] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e12c07u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27535007399253] = TemplatedLoader<0, 0x002a2a00u, 0x40e12c07u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_T2_mtx0_1_I8_flt_T3_mtx0_1_I8_flt_ num_verts= 7792503
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49184279794953] = TemplatedLoader<0x301, 0x00aa2a00u, 0x41214c09u, 0x00241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49184279794953] = TemplatedLoader<0, 0x00aa2a00u, 0x41214c09u, 0x00241209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_ num_verts= 6997347
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20962875526712] = TemplatedLoader<0x301, 0x00032a00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20962875526712] = TemplatedLoader<0, 0x00032a00u, 0x40e12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_u16_ num_verts= 6959346
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27535007258965] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e12c07u, 0x00000a07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27535007258965] = TemplatedLoader<0, 0x002a2a00u, 0x40e12c07u, 0x00000a07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I16_s16_ num_verts= 6775209
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[30231924746487] = TemplatedLoader<0x301, 0x003a2b00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[30231924746487] = TemplatedLoader<0, 0x003a2b00u, 0x40e12c09u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 6605858
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063458455781] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec09u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063458455781] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec09u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_T3_mtx0_1_I8_s16_ num_verts= 5479404
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[52475654983671] = TemplatedLoader<0x301, 0x00be2a00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[52475654983671] = TemplatedLoader<0, 0x00be2a00u, 0x40e12c09u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_flt_C0_0_I16_8888_T0_mtx0_1_I16_flt_ num_verts= 5087888
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21055594987320] = TemplatedLoader<0x301, 0x00033f00u, 0x41215009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21055594987320] = TemplatedLoader<0, 0x00033f00u, 0x41215009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_ num_verts= 4712628
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142488821315] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e12c07u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142488821315] = TemplatedLoader<0, 0x000a2a00u, 0x40e12c07u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_0_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 4596624
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20867912194616] = TemplatedLoader<0x301, 0x00022200u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20867912194616] = TemplatedLoader<0, 0x00022200u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_ num_verts= 4476036
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715636444728] = TemplatedLoader<0x301, 0x00022a00u, 0x40a12c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715636444728] = TemplatedLoader<0, 0x00022a00u, 0x40a12c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_s16_T2_mtx0_1_I16_s16_ num_verts= 4026100
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[31292313582165] = TemplatedLoader<0x301, 0x003f2f00u, 0x40a0ec07u, 0x80000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[31292313582165] = TemplatedLoader<0, 0x003f2f00u, 0x40a0ec07u, 0x80000e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 3933866
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715290456982] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0e407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715290456982] = TemplatedLoader<0, 0x00022a00u, 0x40a0e407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 3843394
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088257084984] = TemplatedLoader<0x301, 0x00022a00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088257084984] = TemplatedLoader<0, 0x00022a00u, 0x40e0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 3507072
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142488821589] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e12c07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142488821589] = TemplatedLoader<0, 0x000a2a00u, 0x40e12c07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 3205767
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142181310293] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e0ec07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142181310293] = TemplatedLoader<0, 0x000a2a00u, 0x40e0ec07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 2731820
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22360955010133] = TemplatedLoader<0x301, 0x000a2f00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22360955010133] = TemplatedLoader<0, 0x000a2f00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 2618200
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22985420527701] = TemplatedLoader<0x301, 0x000f2b00u, 0x40e0ec07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22985420527701] = TemplatedLoader<0, 0x000f2b00u, 0x40e0ec07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 2284636
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21091548379286] = TemplatedLoader<0x301, 0x00022f00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21091548379286] = TemplatedLoader<0, 0x00022f00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_ num_verts= 2095656
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27456284404453] = TemplatedLoader<0x301, 0x002a2a00u, 0x40a12c09u, 0x00000a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27456284404453] = TemplatedLoader<0, 0x002a2a00u, 0x40a12c09u, 0x00000a05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_6666_T0_mtx0_1_I8_u16_ num_verts= 1974560
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20710312655416] = TemplatedLoader<0x301, 0x00022200u, 0x40a12009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20710312655416] = TemplatedLoader<0, 0x00022200u, 0x40a12009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 1962219
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063765967351] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a12c09u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063765967351] = TemplatedLoader<0, 0x000a2a00u, 0x40a12c09u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_ num_verts= 1908924
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22221365506825] = TemplatedLoader<0x301, 0x000a2a00u, 0x41214c09u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22221365506825] = TemplatedLoader<0, 0x000a2a00u, 0x41214c09u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 1783914
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010192422038] = TemplatedLoader<0x301, 0x00022b00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010192422038] = TemplatedLoader<0, 0x00022b00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_ num_verts= 1746992
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27534699887957] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e0ec07u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27534699887957] = TemplatedLoader<0, 0x002a2a00u, 0x40e0ec07u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 1727120
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21012167221142] = TemplatedLoader<0x301, 0x00022e00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21012167221142] = TemplatedLoader<0, 0x00022e00u, 0x40a0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_s16_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_ num_verts= 1637484
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20688359194262] = TemplatedLoader<0x301, 0x00011100u, 0x41216007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20688359194262] = TemplatedLoader<0, 0x00011100u, 0x41216007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx1_1_I8_s16_T3_mtx0_1_I8_u16_ num_verts= 1485846
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49323735788199] = TemplatedLoader<0x301, 0x00aa2f04u, 0x40a0ec07u, 0x80140e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49323735788199] = TemplatedLoader<0, 0x00aa2f04u, 0x40a0ec07u, 0x80140e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_u16_T4_mtx0_1_I8_u16_ num_verts= 1456238
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[135477487882231] = TemplatedLoader<0x301, 0x02aa2a00u, 0x40e12c09u, 0x28140e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[135477487882231] = TemplatedLoader<0, 0x02aa2a00u, 0x40e12c09u, 0x28140e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 1403200
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22058134628931] = TemplatedLoader<0x301, 0x000a2200u, 0x40a0e007u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22058134628931] = TemplatedLoader<0, 0x000a2200u, 0x40a0e007u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_0_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_T2_mtx0_1_I8_flt_ num_verts= 1369444
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27608560435465] = TemplatedLoader<0x301, 0x002a2200u, 0x41214009u, 0x00001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27608560435465] = TemplatedLoader<0, 0x002a2200u, 0x41214009u, 0x00001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_ num_verts= 1350116
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27534968997879] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e12409u, 0x00000e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27534968997879] = TemplatedLoader<0, 0x002a2a00u, 0x40e12409u, 0x00000e07u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_u16_ num_verts= 1316588
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27750182115669] = TemplatedLoader<0x301, 0x002a2a00u, 0x40a0ec07u, 0x80000a07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27750182115669] = TemplatedLoader<0, 0x002a2a00u, 0x40a0ec07u, 0x80000a07u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_0_I16_8888_T0_mtx0_1_I16_flt_ num_verts= 1268552
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21047618913080] = TemplatedLoader<0x301, 0x00033300u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21047618913080] = TemplatedLoader<0, 0x00033300u, 0x41214009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_Dir_u8_ num_verts= 1141844
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20361244090680] = TemplatedLoader<0x301, 0x00010100u, 0x40200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20361244090680] = TemplatedLoader<0, 0x00010100u, 0x40200009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_s16_ num_verts= 1075342
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49105023630677] = TemplatedLoader<0x301, 0x00aa2a00u, 0x40e0ec07u, 0x001c0e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49105023630677] = TemplatedLoader<0, 0x00aa2a00u, 0x40e0ec07u, 0x001c0e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 1010578
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20868065950264] = TemplatedLoader<0x301, 0x00022200u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20868065950264] = TemplatedLoader<0, 0x00022200u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx1_1_I8_s16_T3_mtx0_1_I8_u16_ num_verts= 838856
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49320444456359] = TemplatedLoader<0x301, 0x00aa2a04u, 0x40a0ec07u, 0x80140e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49320444456359] = TemplatedLoader<0, 0x00aa2a04u, 0x40a0ec07u, 0x80140e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_ num_verts= 834394
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27455976995907] = TemplatedLoader<0x301, 0x002a2a00u, 0x40a0ec07u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27455976995907] = TemplatedLoader<0, 0x002a2a00u, 0x40a0ec07u, 0x00000e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_ num_verts= 821856
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22221538481929] = TemplatedLoader<0x301, 0x000a2a00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22221538481929] = TemplatedLoader<0, 0x000a2a00u, 0x41217009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 743474
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21362423993865] = TemplatedLoader<0x301, 0x00051100u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21362423993865] = TemplatedLoader<0, 0x00051100u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 743178
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088915313814] = TemplatedLoader<0x301, 0x00022b00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088915313814] = TemplatedLoader<0, 0x00022b00u, 0x40e0ec07u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_ num_verts= 742966
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21906012671681] = TemplatedLoader<0x301, 0x000a2a00u, 0x4020ec09u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21906012671681] = TemplatedLoader<0, 0x000a2a00u, 0x4020ec09u, 0x00000001u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_u8_T4_mtx0_1_I8_s16_ num_verts= 740460
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[135514119884791] = TemplatedLoader<0x301, 0x02aa2a00u, 0x40e12c09u, 0x38040e07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[135514119884791] = TemplatedLoader<0, 0x02aa2a00u, 0x40e12c09u, 0x38040e07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u8_ num_verts= 641732
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558190661176] = TemplatedLoader<0x301, 0x00022a00u, 0x40212c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558190661176] = TemplatedLoader<0, 0x00022a00u, 0x40212c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_s16_ num_verts= 632800
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23200244629333] = TemplatedLoader<0x301, 0x000f2a00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23200244629333] = TemplatedLoader<0, 0x000f2a00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 543004
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142450420215] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e12409u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142450420215] = TemplatedLoader<0, 0x000a2a00u, 0x40e12409u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 516847
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22357625276645] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0e409u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22357625276645] = TemplatedLoader<0, 0x000a2a00u, 0x40a0e409u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_Dir_u8_ num_verts= 393828
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20373467664696] = TemplatedLoader<0x301, 0x00011100u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20373467664696] = TemplatedLoader<0, 0x00011100u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_u16_ num_verts= 271502
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27456284404727] = TemplatedLoader<0x301, 0x002a2a00u, 0x40a12c09u, 0x00000a07u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27456284404727] = TemplatedLoader<0, 0x002a2a00u, 0x40a12c09u, 0x00000a07u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_u8_ num_verts= 271502
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558152222264] = TemplatedLoader<0x301, 0x00022a00u, 0x40212409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558152222264] = TemplatedLoader<0, 0x00022a00u, 0x40212409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 247036
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088218608534] = TemplatedLoader<0x301, 0x00022a00u, 0x40e0e407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088218608534] = TemplatedLoader<0, 0x00022a00u, 0x40e0e407u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 245552
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20718620227734] = TemplatedLoader<0x301, 0x00022f00u, 0x40a0ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20718620227734] = TemplatedLoader<0, 0x00022f00u, 0x40a0ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_u16_T3_mtx0_1_I8_u16_ num_verts= 240948
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49026228770883] = TemplatedLoader<0x301, 0x00aa2a00u, 0x40a0ec07u, 0x00140a05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49026228770883] = TemplatedLoader<0, 0x00aa2a00u, 0x40a0ec07u, 0x00140a05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_s8_ num_verts= 200384
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20636567565206] = TemplatedLoader<0x301, 0x00022a00u, 0x4060e407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20636567565206] = TemplatedLoader<0, 0x00022a00u, 0x4060e407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_s16_C0_1_I8_4444_ num_verts= 177536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20176165477957] = TemplatedLoader<0x301, 0x00002200u, 0x4000e006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20176165477957] = TemplatedLoader<0, 0x00002200u, 0x4000e006u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 170008
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063458418517] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec07u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063458418517] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec07u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_T0_mtx0_1_Dir_flt_T1_mtx0_1_Dir_flt_ num_verts= 167728
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21350200401080] = TemplatedLoader<0x301, 0x00050100u, 0x41200008u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21350200401080] = TemplatedLoader<0, 0x00050100u, 0x41200008u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I16_u16_ num_verts= 163464
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21181341715256] = TemplatedLoader<0x301, 0x00032f00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21181341715256] = TemplatedLoader<0, 0x00032f00u, 0x40a0ec09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 157620
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22437044873975] = TemplatedLoader<0x301, 0x000a2b00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22437044873975] = TemplatedLoader<0, 0x000a2b00u, 0x40e0ec09u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_s16_ num_verts= 152708
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20153366417046] = TemplatedLoader<0x301, 0x00000100u, 0x40000007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20153366417046] = TemplatedLoader<0, 0x00000100u, 0x40000007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 150102
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063458418243] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec07u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063458418243] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec07u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx1_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 147352
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22360955009859] = TemplatedLoader<0x301, 0x000a2f00u, 0x40a0ec07u, 0x80000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22360955009859] = TemplatedLoader<0, 0x000a2f00u, 0x40a0ec07u, 0x80000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u8_ num_verts= 144345
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20552559323030] = TemplatedLoader<0x301, 0x00022200u, 0x4020e007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20552559323030] = TemplatedLoader<0, 0x00022200u, 0x4020e007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 126720
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794013348758] = TemplatedLoader<0x301, 0x00022a00u, 0x40e0e407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794013348758] = TemplatedLoader<0, 0x00022a00u, 0x40e0e407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_ num_verts= 123646
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22436386570069] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e0ec07u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22436386570069] = TemplatedLoader<0, 0x000a2a00u, 0x40e0ec07u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 121136
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22066749750083] = TemplatedLoader<0x301, 0x000a2f00u, 0x40a0ec07u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22066749750083] = TemplatedLoader<0, 0x000a2f00u, 0x40a0ec07u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_ num_verts= 98728
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27535007436517] = TemplatedLoader<0x301, 0x002a2a00u, 0x40e12c09u, 0x00000e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27535007436517] = TemplatedLoader<0, 0x002a2a00u, 0x40e12c09u, 0x00000e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_u16_ num_verts= 93702
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49026228911171] = TemplatedLoader<0x301, 0x00aa2a00u, 0x40a0ec07u, 0x00140e05u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49026228911171] = TemplatedLoader<0, 0x00aa2a00u, 0x40a0ec07u, 0x00140e05u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u8_ num_verts= 87682
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20557883112342] = TemplatedLoader<0x301, 0x00022a00u, 0x4020ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20557883112342] = TemplatedLoader<0, 0x00022a00u, 0x4020ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 86352
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21009495716758] = TemplatedLoader<0x301, 0x00022a00u, 0x40a0e407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21009495716758] = TemplatedLoader<0, 0x00022a00u, 0x40a0e407u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 81360
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22357663678293] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22357663678293] = TemplatedLoader<0, 0x000a2a00u, 0x40a0ec07u, 0x80000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u8_ num_verts= 69352
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20557844673430] = TemplatedLoader<0x301, 0x00022a00u, 0x4020e407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20557844673430] = TemplatedLoader<0, 0x00022a00u, 0x4020e407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_ num_verts= 65098
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22142181310019] = TemplatedLoader<0x301, 0x000a2a00u, 0x40e0ec07u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22142181310019] = TemplatedLoader<0, 0x000a2a00u, 0x40e0ec07u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_ num_verts= 57056
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22063420016869] = TemplatedLoader<0x301, 0x000a2a00u, 0x40a0e409u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22063420016869] = TemplatedLoader<0, 0x000a2a00u, 0x40a0e409u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_u16_ num_verts= 49400
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21004210366358] = TemplatedLoader<0x301, 0x00022200u, 0x40a0e007u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21004210366358] = TemplatedLoader<0, 0x00022200u, 0x40a0e007u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 47788
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20710620166712] = TemplatedLoader<0x301, 0x00022200u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20710620166712] = TemplatedLoader<0, 0x00022200u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 39984
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20873408959032] = TemplatedLoader<0x301, 0x00022a00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20873408959032] = TemplatedLoader<0, 0x00022a00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 37856
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20789343058488] = TemplatedLoader<0x301, 0x00022200u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20789343058488] = TemplatedLoader<0, 0x00022200u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 34784
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21167441243704] = TemplatedLoader<0x301, 0x00022a00u, 0x41214c09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21167441243704] = TemplatedLoader<0, 0x00022a00u, 0x41214c09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_ num_verts= 33660
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20182104346168] = TemplatedLoader<0x301, 0x00002a00u, 0x40016c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20182104346168] = TemplatedLoader<0, 0x00002a00u, 0x40016c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I16_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 33098
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20796684853142] = TemplatedLoader<0x301, 0x00022e00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20796684853142] = TemplatedLoader<0, 0x00022e00u, 0x40e0ec07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_u8_ num_verts= 18704
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558152184726] = TemplatedLoader<0x301, 0x00022a00u, 0x40212407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558152184726] = TemplatedLoader<0, 0x00022a00u, 0x40212407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_u8_ num_verts= 16536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558152203495] = TemplatedLoader<0x301, 0x00022a00u, 0x40212408u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558152203495] = TemplatedLoader<0, 0x00022a00u, 0x40212408u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u8_ num_verts= 10792
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20852049933206] = TemplatedLoader<0x301, 0x00022a00u, 0x4020e407u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20852049933206] = TemplatedLoader<0, 0x00022a00u, 0x4020e407u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_6666_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_T2_mtx0_1_I8_s16_T3_mtx0_1_I8_s16_T4_mtx0_1_I8_s16_T5_mtx0_1_I8_s16_ num_verts= 10360
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[480635454474455] = TemplatedLoader<0x301, 0x0aaa2a00u, 0x40e12409u, 0x381c0e07u, 0x000000e0u>;
	}
	else
#endif
	{
		pvlmap[480635454474455] = TemplatedLoader<0, 0x0aaa2a00u, 0x40e12409u, 0x381c0e07u, 0x000000e0u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_4444_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_ num_verts= 9684
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21905974195231] = TemplatedLoader<0x301, 0x000a2a00u, 0x4020e407u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21905974195231] = TemplatedLoader<0, 0x000a2a00u, 0x4020e407u, 0x00000001u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_s16_T0_mtx0_1_Dir_flt_ num_verts= 3540
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676135620246] = TemplatedLoader<0x301, 0x00010100u, 0x41200007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676135620246] = TemplatedLoader<0, 0x00010100u, 0x41200007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_C0_1_I8_4444_T0_mtx0_1_I8_s16_ num_verts= 1280
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20788727998358] = TemplatedLoader<0x301, 0x00022200u, 0x40e0e007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20788727998358] = TemplatedLoader<0, 0x00022200u, 0x40e0e007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_s16_T0_mtx0_1_Dir_s16_ num_verts= 320
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20597412728470] = TemplatedLoader<0x301, 0x00010100u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20597412728470] = TemplatedLoader<0, 0x00010100u, 0x40e00007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 96
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20553174383160] = TemplatedLoader<0x301, 0x00022200u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20553174383160] = TemplatedLoader<0, 0x00022200u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
}
