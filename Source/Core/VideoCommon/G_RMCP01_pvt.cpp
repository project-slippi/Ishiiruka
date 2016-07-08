// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Added for Ishiiruka by Tino
#include "VideoCommon/G_RMCP01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"


void G_RMCP01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_s16_ num_verts= 465862113
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20958517515064] = TemplatedLoader<0x301, 0x00032300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20958517515064] = TemplatedLoader<0, 0x00032300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 224489078
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20790001324856] = TemplatedLoader<0x301, 0x00022300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20790001324856] = TemplatedLoader<0, 0x00022300u, 0x40e16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_ num_verts= 177866519
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20966474369848] = TemplatedLoader<0x301, 0x00032f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20966474369848] = TemplatedLoader<0, 0x00032f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_ num_verts= 120296975
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21037240406840] = TemplatedLoader<0x301, 0x00032300u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21037240406840] = TemplatedLoader<0, 0x00032300u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 111042987
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794666810262] = TemplatedLoader<0x301, 0x00022a00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794666810262] = TemplatedLoader<0, 0x00022a00u, 0x40e16c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 107191361
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260695690271] = TemplatedLoader<0x301, 0x00032f06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260695690271] = TemplatedLoader<0, 0x00032f06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_s16_ num_verts= 102595033
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22980711798519] = TemplatedLoader<0x301, 0x000f2300u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22980711798519] = TemplatedLoader<0, 0x000f2300u, 0x40e16009u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_u16_T1_mtx0_1_I16_s16_ num_verts= 96096614
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22901988906743] = TemplatedLoader<0x301, 0x000f2300u, 0x40a16009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22901988906743] = TemplatedLoader<0, 0x000f2300u, 0x40a16009u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 80184032
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21181972798495] = TemplatedLoader<0x301, 0x00032f06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21181972798495] = TemplatedLoader<0, 0x00032f06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_ num_verts= 72884028
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693188082582] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693188082582] = TemplatedLoader<0, 0x00020a00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_s16_ num_verts= 69327708
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771910974358] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771910974358] = TemplatedLoader<0, 0x00020a00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_ num_verts= 63632016
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159348510264] = TemplatedLoader<0x301, 0x00000a00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159348510264] = TemplatedLoader<0, 0x00000a00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u16_ num_verts= 63032684
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22980711798245] = TemplatedLoader<0x301, 0x000f2300u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22980711798245] = TemplatedLoader<0, 0x000f2300u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s8_Nrm_0_0_I8_s8_ num_verts= 50593152
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159309958738] = TemplatedLoader<0x301, 0x00000a00u, 0x40000403u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159309958738] = TemplatedLoader<0, 0x00000a00u, 0x40000403u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 45272968
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20711278433080] = TemplatedLoader<0x301, 0x00022300u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20711278433080] = TemplatedLoader<0, 0x00022300u, 0x40a16009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 40102650
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20868724216632] = TemplatedLoader<0x301, 0x00022300u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20868724216632] = TemplatedLoader<0, 0x00022300u, 0x41216009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_I8_u8_ num_verts= 29351924
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20529760280888] = TemplatedLoader<0x301, 0x00020100u, 0x40200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20529760280888] = TemplatedLoader<0, 0x00020100u, 0x40200009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_ num_verts= 25099825
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23059434690569] = TemplatedLoader<0x301, 0x000f2300u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23059434690569] = TemplatedLoader<0, 0x000f2300u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_T2_mtx1_1_Inv_flt_ num_verts= 21661560
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21181967654556] = TemplatedLoader<0x301, 0x00032f04u, 0x40a16c09u, 0x80001200u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21181967654556] = TemplatedLoader<0, 0x00032f04u, 0x40a16c09u, 0x80001200u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_flt_ num_verts= 15455418
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21339402521400] = TemplatedLoader<0x301, 0x00032f00u, 0x41216c09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21339402521400] = TemplatedLoader<0, 0x00032f00u, 0x41216c09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_6666_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 14801498
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260388178975] = TemplatedLoader<0x301, 0x00032f06u, 0x40e12c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260388178975] = TemplatedLoader<0, 0x00032f06u, 0x40e12c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u16_ num_verts= 14798244
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22137472580837] = TemplatedLoader<0x301, 0x000a2200u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22137472580837] = TemplatedLoader<0, 0x000a2200u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 11907875
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20876681071416] = TemplatedLoader<0x301, 0x00022f00u, 0x41216c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20876681071416] = TemplatedLoader<0, 0x00022f00u, 0x41216c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_ num_verts= 11577515
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693188120120] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693188120120] = TemplatedLoader<0, 0x00020a00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_ num_verts= 10503075
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22216195473161] = TemplatedLoader<0x301, 0x000a2200u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22216195473161] = TemplatedLoader<0, 0x000a2200u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 9066593
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794666847800] = TemplatedLoader<0x301, 0x00022a00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794666847800] = TemplatedLoader<0, 0x00022a00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I8_565_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_ num_verts= 8780564
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21258993461435] = TemplatedLoader<0x301, 0x00032f02u, 0x40e00c09u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21258993461435] = TemplatedLoader<0, 0x00032f02u, 0x40e00c09u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_ num_verts= 8758000
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159367729720] = TemplatedLoader<0x301, 0x00000a00u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159367729720] = TemplatedLoader<0, 0x00000a00u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 7284492
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715943956024] = TemplatedLoader<0x301, 0x00022a00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715943956024] = TemplatedLoader<0, 0x00022a00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_ num_verts= 6567880
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20887751478072] = TemplatedLoader<0x301, 0x00032f00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20887751478072] = TemplatedLoader<0, 0x00032f00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 6502793
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088888168223] = TemplatedLoader<0x301, 0x00022a06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088888168223] = TemplatedLoader<0, 0x00022a06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_s16_ num_verts= 5397636
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22058749689335] = TemplatedLoader<0x301, 0x000a2200u, 0x40a16009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22058749689335] = TemplatedLoader<0, 0x000a2200u, 0x40a16009u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 5316720
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21092179500063] = TemplatedLoader<0x301, 0x00022f06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21092179500063] = TemplatedLoader<0, 0x00022f06u, 0x40e16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 4836540
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20795325114168] = TemplatedLoader<0x301, 0x00022b00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20795325114168] = TemplatedLoader<0, 0x00022b00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_ num_verts= 4594382
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159348472726] = TemplatedLoader<0x301, 0x00000a00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159348472726] = TemplatedLoader<0, 0x00000a00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I8_565_T0_mtx0_1_I16_s16_ num_verts= 4585368
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20964783057720] = TemplatedLoader<0x301, 0x00032f00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20964783057720] = TemplatedLoader<0, 0x00032f00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_ num_verts= 4405056
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159310033814] = TemplatedLoader<0x301, 0x00000a00u, 0x40000407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159310033814] = TemplatedLoader<0, 0x00000a00u, 0x40000407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 4123736
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260714909727] = TemplatedLoader<0x301, 0x00032f06u, 0x40e17009u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260714909727] = TemplatedLoader<0, 0x00032f06u, 0x40e17009u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_s16_T0_mtx0_1_Dir_u16_ num_verts= 4049624
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20518689817925] = TemplatedLoader<0x301, 0x00010100u, 0x40a00006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20518689817925] = TemplatedLoader<0, 0x00010100u, 0x40a00006u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 3978722
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715943918486] = TemplatedLoader<0x301, 0x00022a00u, 0x40a16c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715943918486] = TemplatedLoader<0, 0x00022a00u, 0x40a16c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_s16_ num_verts= 3498130
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771911011896] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771911011896] = TemplatedLoader<0, 0x00020a00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 3064958
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715905517112] = TemplatedLoader<0x301, 0x00022a00u, 0x40a16409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715905517112] = TemplatedLoader<0, 0x00022a00u, 0x40a16409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_s16_ num_verts= 3057252
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771930231352] = TemplatedLoader<0x301, 0x00020a00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771930231352] = TemplatedLoader<0, 0x00020a00u, 0x40e01009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_Dir_flt_T0_mtx0_1_Dir_s8_ num_verts= 2951744
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20439966982456] = TemplatedLoader<0x301, 0x00010100u, 0x40600009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20439966982456] = TemplatedLoader<0, 0x00010100u, 0x40600009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 2750896
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558459733560] = TemplatedLoader<0x301, 0x00022a00u, 0x40216409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558459733560] = TemplatedLoader<0, 0x00022a00u, 0x40216409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I8_565_T0_mtx0_1_I8_s16_T1_mtx1_1_Inv_flt_ num_verts= 2580333
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21090477271227] = TemplatedLoader<0x301, 0x00022f02u, 0x40e00c09u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21090477271227] = TemplatedLoader<0, 0x00022f02u, 0x40e00c09u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_2_I8_u8_T0_mtx0_1_I8_u8_ num_verts= 2540536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20530418378335] = TemplatedLoader<0x301, 0x00020200u, 0x40200000u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20530418378335] = TemplatedLoader<0, 0x00020200u, 0x40200000u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_ num_verts= 2385726
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21009121313979] = TemplatedLoader<0x301, 0x00022b02u, 0x40a00c09u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21009121313979] = TemplatedLoader<0, 0x00022b02u, 0x40a00c09u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 2208268
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558459696022] = TemplatedLoader<0x301, 0x00022a00u, 0x40216407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558459696022] = TemplatedLoader<0, 0x00022a00u, 0x40216407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 2116752
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20719235287864] = TemplatedLoader<0x301, 0x00022f00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20719235287864] = TemplatedLoader<0, 0x00022f00u, 0x40a16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I16_u16_ num_verts= 1850220
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22812195608037] = TemplatedLoader<0x301, 0x000e2300u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22812195608037] = TemplatedLoader<0, 0x000e2300u, 0x40e16009u, 0x00000005u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_s8_ num_verts= 1590540
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20632555541304] = TemplatedLoader<0x301, 0x00022300u, 0x40616009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20632555541304] = TemplatedLoader<0, 0x00022300u, 0x40616009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 1439614
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20797958179640] = TemplatedLoader<0x301, 0x00022f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20797958179640] = TemplatedLoader<0, 0x00022f00u, 0x40e16c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_ num_verts= 1369896
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20159310071352] = TemplatedLoader<0x301, 0x00000a00u, 0x40000409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20159310071352] = TemplatedLoader<0, 0x00000a00u, 0x40000409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_ num_verts= 1229472
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20850653123128] = TemplatedLoader<0x301, 0x00020a00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20850653123128] = TemplatedLoader<0, 0x00020a00u, 0x41201009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_s8_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u8_ num_verts= 1062332
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535703766273] = TemplatedLoader<0x301, 0x00020a00u, 0x40200402u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535703766273] = TemplatedLoader<0, 0x00020a00u, 0x40200402u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I16_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 1039104
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21178681466655] = TemplatedLoader<0x301, 0x00032a06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21178681466655] = TemplatedLoader<0, 0x00032a06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_s16_ num_verts= 936054
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21901303905783] = TemplatedLoader<0x301, 0x000a2200u, 0x40216009u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21901303905783] = TemplatedLoader<0, 0x000a2200u, 0x40216009u, 0x00000007u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_T0_mtx0_1_Dir_flt_ num_verts= 866268
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20676793886614] = TemplatedLoader<0x301, 0x00010200u, 0x41200007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20676793886614] = TemplatedLoader<0, 0x00010200u, 0x41200007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_u16_ num_verts= 652612
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693207339576] = TemplatedLoader<0x301, 0x00020a00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693207339576] = TemplatedLoader<0, 0x00020a00u, 0x40a01009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_ num_verts= 545890
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693846386488] = TemplatedLoader<0x301, 0x00020b00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693846386488] = TemplatedLoader<0, 0x00020b00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 542940
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794628408888] = TemplatedLoader<0x301, 0x00022a00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794628408888] = TemplatedLoader<0, 0x00022a00u, 0x40e16409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_ num_verts= 482100
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20154024720952] = TemplatedLoader<0x301, 0x00000200u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20154024720952] = TemplatedLoader<0, 0x00000200u, 0x40000009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_flt_ num_verts= 422770
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20873389739576] = TemplatedLoader<0x301, 0x00022a00u, 0x41216c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20873389739576] = TemplatedLoader<0, 0x00022a00u, 0x41216c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 401448
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558498134934] = TemplatedLoader<0x301, 0x00022a00u, 0x40216c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558498134934] = TemplatedLoader<0, 0x00022a00u, 0x40216c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 400960
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20987409440543] = TemplatedLoader<0x301, 0x00020a06u, 0x40a00c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20987409440543] = TemplatedLoader<0, 0x00020a06u, 0x40a00c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u8_ num_verts= 347328
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535703860118] = TemplatedLoader<0x301, 0x00020a00u, 0x40200407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535703860118] = TemplatedLoader<0, 0x00020a00u, 0x40200407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 328190
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21013456608287] = TemplatedLoader<0x301, 0x00022f06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21013456608287] = TemplatedLoader<0, 0x00022f06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 307666
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20789343020950] = TemplatedLoader<0x301, 0x00022200u, 0x40e16007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20789343020950] = TemplatedLoader<0, 0x00022200u, 0x40e16007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_1_I8_8888_ num_verts= 274480
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20182065907256] = TemplatedLoader<0x301, 0x00002a00u, 0x40016409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20182065907256] = TemplatedLoader<0, 0x00002a00u, 0x40016409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 212788
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794628371350] = TemplatedLoader<0x301, 0x00022a00u, 0x40e16407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794628371350] = TemplatedLoader<0, 0x00022a00u, 0x40e16407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 199600
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794686029718] = TemplatedLoader<0x301, 0x00022a00u, 0x40e17007u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794686029718] = TemplatedLoader<0, 0x00022a00u, 0x40e17007u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_u8_ num_verts= 184516
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20154024552031] = TemplatedLoader<0x301, 0x00000200u, 0x40000000u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20154024552031] = TemplatedLoader<0, 0x00000200u, 0x40000000u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_u8_T0_mtx0_1_I8_u8_T1_mtx0_1_Dir_flt_ num_verts= 173728
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21204483140400] = TemplatedLoader<0x301, 0x00060200u, 0x40200000u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21204483140400] = TemplatedLoader<0, 0x00060200u, 0x40200000u, 0x00000009u, 0x00000000u>;
	}
	//P_mtx0_2_I8_s16_T0_mtx0_1_I8_u8_ num_verts= 173664
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20530418490949] = TemplatedLoader<0x301, 0x00020200u, 0x40200006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20530418490949] = TemplatedLoader<0, 0x00020200u, 0x40200006u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 166286
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010165238909] = TemplatedLoader<0x301, 0x00022a06u, 0x40a16c07u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010165238909] = TemplatedLoader<0, 0x00022a06u, 0x40a16c07u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_2_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 127376
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20553174364391] = TemplatedLoader<0x301, 0x00022200u, 0x40216008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20553174364391] = TemplatedLoader<0, 0x00022200u, 0x40216008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_T0_mtx0_1_I8_u16_ num_verts= 113935
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20715905479574] = TemplatedLoader<0x301, 0x00022a00u, 0x40a16407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20715905479574] = TemplatedLoader<0, 0x00022a00u, 0x40a16407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_u8_ num_verts= 84672
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20556806822806] = TemplatedLoader<0x301, 0x00022a00u, 0x40200c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20556806822806] = TemplatedLoader<0, 0x00022a00u, 0x40200c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_u16_ num_verts= 50141
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20714252643896] = TemplatedLoader<0x301, 0x00022a00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20714252643896] = TemplatedLoader<0, 0x00022a00u, 0x40a00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_u16_ num_verts= 47556
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20714214204984] = TemplatedLoader<0x301, 0x00022a00u, 0x40a00409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20714214204984] = TemplatedLoader<0, 0x00022a00u, 0x40a00409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 39304
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20794686067256] = TemplatedLoader<0x301, 0x00022a00u, 0x40e17009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20794686067256] = TemplatedLoader<0, 0x00022a00u, 0x40e17009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_flt_C0_1_I8_8888_ num_verts= 34370
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20177438823224] = TemplatedLoader<0x301, 0x00002300u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20177438823224] = TemplatedLoader<0, 0x00002300u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_s16_ num_verts= 31704
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20792975535672] = TemplatedLoader<0x301, 0x00022a00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20792975535672] = TemplatedLoader<0, 0x00022a00u, 0x40e00c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I16_s16_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_s16_ num_verts= 31536
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20793633764502] = TemplatedLoader<0x301, 0x00022b00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20793633764502] = TemplatedLoader<0, 0x00022b00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_s16_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_ num_verts= 23788
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010154322201] = TemplatedLoader<0x301, 0x00022a02u, 0x40a16c07u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010154322201] = TemplatedLoader<0, 0x00022a02u, 0x40a16c07u, 0x80000009u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_ num_verts= 23264
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771872535446] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771872535446] = TemplatedLoader<0, 0x00020a00u, 0x40e00407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u8_ num_verts= 16280
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20558498172472] = TemplatedLoader<0x301, 0x00022a00u, 0x40216c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20558498172472] = TemplatedLoader<0, 0x00022a00u, 0x40216c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_T0_mtx0_1_I8_flt_ num_verts= 15840
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20850633903672] = TemplatedLoader<0x301, 0x00020a00u, 0x41200c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20850633903672] = TemplatedLoader<0, 0x00020a00u, 0x41200c09u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_ num_verts= 11096
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21088872107576] = TemplatedLoader<0x301, 0x00022a00u, 0x40e16c09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21088872107576] = TemplatedLoader<0, 0x00022a00u, 0x40e16c09u, 0x80000000u, 0x00000000u>;
	}
	//P_mtx0_2_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_Dir_flt_ num_verts= 10820
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20682117657157] = TemplatedLoader<0x301, 0x00010a00u, 0x41200c06u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20682117657157] = TemplatedLoader<0, 0x00010a00u, 0x41200c06u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_s16_ num_verts= 8352
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20792975498134] = TemplatedLoader<0x301, 0x00022a00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20792975498134] = TemplatedLoader<0, 0x00022a00u, 0x40e00c07u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u16_ num_verts= 6044
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693149643670] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693149643670] = TemplatedLoader<0, 0x00020a00u, 0x40a00407u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx1_3_I8_flt_Nrm_0_0_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u16_T1_mtx1_1_Inv_flt_T2_mtx1_1_Inv_flt_ num_verts= 2656
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21010165276447] = TemplatedLoader<0x301, 0x00022a06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21010165276447] = TemplatedLoader<0, 0x00022a06u, 0x40a16c09u, 0x80001209u, 0x00000000u>;
	}
	//P_mtx0_2_Dir_flt_T0_mtx0_1_Dir_s8_ num_verts= 2028
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20439966963687] = TemplatedLoader<0x301, 0x00010100u, 0x40600008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20439966963687] = TemplatedLoader<0, 0x00010100u, 0x40600008u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u16_ num_verts= 368
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693149681208] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00409u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693149681208] = TemplatedLoader<0, 0x00020a00u, 0x40a00409u, 0x00000000u, 0x00000000u>;
	}
	//P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_1_I8_8888_ num_verts= 100
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20182065869718] = TemplatedLoader<0x301, 0x00002a00u, 0x40016407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20182065869718] = TemplatedLoader<0, 0x00002a00u, 0x40016407u, 0x00000000u, 0x00000000u>;
	}
	/*/*/
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_1_I8_8888_T0_mtx0_1_I16_s16_T1_mtx1_1_Inv_flt_
	// num_verts= 40480667
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21260684773563] = TemplatedLoader<0x301, 0x00032f02u, 0x40e16c09u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21260684773563] = TemplatedLoader<0, 0x00032f02u, 0x40e16c09u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_0_I8_565_T0_mtx0_1_I8_u16_
	// num_verts= 1035916
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20714252606358] = TemplatedLoader<0x301, 0x00022a00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20714252606358] = TemplatedLoader<0, 0x00022a00u, 0x40a00c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_s16_C0_0_I8_565_
	// num_verts= 258096
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20180413034040] = TemplatedLoader<0x301, 0x00002a00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20180413034040] = TemplatedLoader<0, 0x00002a00u, 0x40000c09u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s8_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_s16_
	// num_verts= 232
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22120001964224] = TemplatedLoader<0x301, 0x000a0a00u, 0x40e00402u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22120001964224] = TemplatedLoader<0, 0x000a0a00u, 0x40e00402u, 0x00000007u, 0x00000000u>;
	}
}
