// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_RSBP01_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_RSBP01_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_T1_mtx1_1_Inv_flt_
// num_verts= 30974039
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21339426884795] = TemplatedLoader<0x301, 0x00032f02u, 0x41217009u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21339426884795] = TemplatedLoader<0, 0x00032f02u, 0x41217009u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_
// num_verts= 2575754
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20182123565624] = TemplatedLoader<0x301, 0x00002a00u, 0x40017009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20182123565624] = TemplatedLoader<0, 0x00002a00u, 0x40017009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_s16_C0_0_I8_565_
// num_verts= 1925770
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20477909625656] = TemplatedLoader<0x301, 0x00002f00u, 0x40000c09u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20477909625656] = TemplatedLoader<0, 0x00002f00u, 0x40000c09u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I16_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 1767558
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20876042024504] = TemplatedLoader<0x301, 0x00022e00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20876042024504] = TemplatedLoader<0, 0x00022e00u, 0x41217009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx1_1_Inv_flt_
// num_verts= 1625397
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21167619362747] = TemplatedLoader<0x301, 0x00022a02u, 0x41217009u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21167619362747] = TemplatedLoader<0, 0x00022a02u, 0x41217009u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 1324076
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20850653104359] = TemplatedLoader<0x301, 0x00020a00u, 0x41201008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20850653104359] = TemplatedLoader<0, 0x00020a00u, 0x41201008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_T2_mtx0_1_I8_flt_
// num_verts= 1070190
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[27614057199881] = TemplatedLoader<0x301, 0x002a2a00u, 0x41217009u, 0x00001209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[27614057199881] = TemplatedLoader<0, 0x002a2a00u, 0x41217009u, 0x00001209u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I16_flt_
// num_verts= 895442
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21336130409016] = TemplatedLoader<0x301, 0x00032a00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21336130409016] = TemplatedLoader<0, 0x00032a00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_I16_flt_Nrm_0_0_I8_flt_
// num_verts= 525600
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20160025977319] = TemplatedLoader<0x301, 0x00000b00u, 0x40001008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20160025977319] = TemplatedLoader<0, 0x00000b00u, 0x40001008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I16_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 315040
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20851311370727] = TemplatedLoader<0x301, 0x00020b00u, 0x41201008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20851311370727] = TemplatedLoader<0, 0x00020b00u, 0x41201008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_T2_mtx0_1_I8_flt_T3_mtx0_1_I8_flt_
// num_verts= 267388
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[49184452770057] = TemplatedLoader<0x301, 0x00aa2a00u, 0x41217009u, 0x00241209u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[49184452770057] = TemplatedLoader<0, 0x00aa2a00u, 0x41217009u, 0x00241209u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_C0_0_I8_565_
// num_verts= 205232
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20180412996502] = TemplatedLoader<0x301, 0x00002a00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20180412996502] = TemplatedLoader<0, 0x00002a00u, 0x40000c07u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 199470
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21168272485176] = TemplatedLoader<0x301, 0x00022b00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21168272485176] = TemplatedLoader<0, 0x00022b00u, 0x41217009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_T0_mtx0_1_I8_u16_
// num_verts= 166356
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20687864274501] = TemplatedLoader<0x301, 0x00020200u, 0x40a00006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20687864274501] = TemplatedLoader<0, 0x00020200u, 0x40a00006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s16_T0_mtx0_1_I8_u16_T1_mtx0_1_I8_u16_
// num_verts= 145656
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22041317604931] = TemplatedLoader<0x301, 0x000a0a00u, 0x40a00c07u, 0x00000005u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22041317604931] = TemplatedLoader<0, 0x000a0a00u, 0x40a00c07u, 0x00000005u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_
// num_verts= 95860
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20182123546855] = TemplatedLoader<0x301, 0x00002a00u, 0x40017008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20182123546855] = TemplatedLoader<0, 0x00002a00u, 0x40017008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 79491
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20873408940263] = TemplatedLoader<0x301, 0x00022a00u, 0x41217008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20873408940263] = TemplatedLoader<0, 0x00022a00u, 0x41217008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s8_T0_mtx0_1_I8_u8_
// num_verts= 30036
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20530418415873] = TemplatedLoader<0x301, 0x00020200u, 0x40200002u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20530418415873] = TemplatedLoader<0, 0x00020200u, 0x40200002u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_u8_
// num_verts= 27333
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20553174326853] = TemplatedLoader<0x301, 0x00022200u, 0x40216006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20553174326853] = TemplatedLoader<0, 0x00022200u, 0x40216006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 23029
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20845310095591] = TemplatedLoader<0x301, 0x00020200u, 0x41200008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20845310095591] = TemplatedLoader<0, 0x00020200u, 0x41200008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_T0_mtx0_1_I8_u8_T1_mtx0_1_I8_u8_
// num_verts= 18384
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21878548012750] = TemplatedLoader<0x301, 0x000a0200u, 0x40200006u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21878548012750] = TemplatedLoader<0, 0x000a0200u, 0x40200006u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx1_2_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_
// num_verts= 17440
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21144858364135] = TemplatedLoader<0x301, 0x00020a00u, 0x41201008u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21144858364135] = TemplatedLoader<0, 0x00020a00u, 0x41201008u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_u16_T0_mtx0_1_I8_u8_
// num_verts= 15716
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20530418453411] = TemplatedLoader<0x301, 0x00020200u, 0x40200004u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20530418453411] = TemplatedLoader<0, 0x00020200u, 0x40200004u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s8_T0_mtx0_1_I8_u16_
// num_verts= 6594
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20687864199425] = TemplatedLoader<0x301, 0x00020200u, 0x40a00002u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20687864199425] = TemplatedLoader<0, 0x00020200u, 0x40a00002u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_u8_
// num_verts= 5762
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20556768365125] = TemplatedLoader<0x301, 0x00022a00u, 0x40200406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20556768365125] = TemplatedLoader<0, 0x00022a00u, 0x40200406u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 4360
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22198782646025] = TemplatedLoader<0x301, 0x000a0a00u, 0x41201009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22198782646025] = TemplatedLoader<0, 0x000a0a00u, 0x41201009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_flt_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 3500
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22198782627256] = TemplatedLoader<0x301, 0x000a0a00u, 0x41201008u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22198782627256] = TemplatedLoader<0, 0x000a0a00u, 0x41201008u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_
// num_verts= 2736
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20868065931495] = TemplatedLoader<0x301, 0x00022200u, 0x41216008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20868065931495] = TemplatedLoader<0, 0x00022200u, 0x41216008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s8_
// num_verts= 2160
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20614426733125] = TemplatedLoader<0x301, 0x00020a00u, 0x40600406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20614426733125] = TemplatedLoader<0, 0x00020a00u, 0x40600406u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_C0_1_I8_8888_
// num_verts= 1812
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20176780500549] = TemplatedLoader<0x301, 0x00002200u, 0x40016006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20176780500549] = TemplatedLoader<0, 0x00002200u, 0x40016006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_u16_
// num_verts= 768
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20714214148677] = TemplatedLoader<0x301, 0x00022a00u, 0x40a00406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20714214148677] = TemplatedLoader<0, 0x00022a00u, 0x40a00406u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_
// num_verts= 704
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20154024702183] = TemplatedLoader<0x301, 0x00000200u, 0x40000008u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20154024702183] = TemplatedLoader<0, 0x00000200u, 0x40000008u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_s8_
// num_verts= 704
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20635491275670] = TemplatedLoader<0x301, 0x00022a00u, 0x40600407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20635491275670] = TemplatedLoader<0, 0x00022a00u, 0x40600407u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_
// num_verts= 660
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20771872516677] = TemplatedLoader<0x301, 0x00020a00u, 0x40e00406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20771872516677] = TemplatedLoader<0, 0x00020a00u, 0x40e00406u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_u16_
// num_verts= 336
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20714214186215] = TemplatedLoader<0x301, 0x00022a00u, 0x40a00408u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20714214186215] = TemplatedLoader<0, 0x00022a00u, 0x40a00408u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s8_
// num_verts= 224
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20614426770663] = TemplatedLoader<0x301, 0x00020a00u, 0x40600408u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20614426770663] = TemplatedLoader<0, 0x00020a00u, 0x40600408u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u16_
// num_verts= 216
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20693149624901] = TemplatedLoader<0x301, 0x00020a00u, 0x40a00406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20693149624901] = TemplatedLoader<0, 0x00020a00u, 0x40a00406u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_flt_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 200
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22193439618488] = TemplatedLoader<0x301, 0x000a0200u, 0x41200008u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22193439618488] = TemplatedLoader<0, 0x000a0200u, 0x41200008u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_Nrm_0_0_I8_s8_C0_0_I8_565_T0_mtx0_1_I8_u8_
// num_verts= 140
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20556768383894] = TemplatedLoader<0x301, 0x00022a00u, 0x40200407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20556768383894] = TemplatedLoader<0, 0x00022a00u, 0x40200407u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s8_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u8_
// num_verts= 120
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535703785042] = TemplatedLoader<0x301, 0x00020a00u, 0x40200403u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535703785042] = TemplatedLoader<0, 0x00020a00u, 0x40200403u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_u8_
// num_verts= 48
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20535703841349] = TemplatedLoader<0x301, 0x00020a00u, 0x40200406u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20535703841349] = TemplatedLoader<0, 0x00020a00u, 0x40200406u, 0x00000000u, 0x00000000u>;
	}
}
