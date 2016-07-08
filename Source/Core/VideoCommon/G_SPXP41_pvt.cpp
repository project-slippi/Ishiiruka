// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_SPXP41_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_SPXP41_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx0_3_I16_s16_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u8_
// num_verts= 1386987325
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22957955924255] = TemplatedLoader<0x301, 0x000f0300u, 0x40e00007u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22957955924255] = TemplatedLoader<0, 0x000f0300u, 0x40e00007u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_flt_T7_mtx1_1_Inv_flt_
// num_verts= 907716596
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21317070535608] = TemplatedLoader<0x301, 0x00030f80u, 0x41201009u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21317070535608] = TemplatedLoader<0, 0x00030f80u, 0x41201009u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx0_3_I16_s16_C0_1_I16_8888_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u8_
// num_verts= 574119742
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22991244022047] = TemplatedLoader<0x301, 0x000f3300u, 0x40e16007u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22991244022047] = TemplatedLoader<0, 0x000f3300u, 0x40e16007u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
// num_verts= 522350893
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23036678854665] = TemplatedLoader<0x301, 0x000f0300u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23036678854665] = TemplatedLoader<0, 0x000f0300u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx1_1_I16_flt_
// num_verts= 121562691
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21316668476305] = TemplatedLoader<0x301, 0x00030f01u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21316668476305] = TemplatedLoader<0, 0x00030f01u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s8_C0_1_I16_8888_T0_mtx0_1_I16_s16_
// num_verts= 99148371
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20976968155286] = TemplatedLoader<0x301, 0x00033f00u, 0x40e16407u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20976968155286] = TemplatedLoader<0, 0x00033f00u, 0x40e16407u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I8_s16_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u8_
// num_verts= 66794075
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22114716706847] = TemplatedLoader<0x301, 0x000a0200u, 0x40e00007u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22114716706847] = TemplatedLoader<0, 0x000a0200u, 0x40e00007u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx0_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx0_1_I16_s16_T1_mtx0_1_I16_u8_
// num_verts= 60433657
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22965874340127] = TemplatedLoader<0x301, 0x000f0f00u, 0x40e00407u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22965874340127] = TemplatedLoader<0, 0x000f0f00u, 0x40e00407u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_flt_T7_mtx1_1_Inv_flt_
// num_verts= 37901628
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21342382559160] = TemplatedLoader<0x301, 0x00033380u, 0x41216009u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21342382559160] = TemplatedLoader<0, 0x00033380u, 0x41216009u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx0_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T1_mtx0_1_I8_u8_
// num_verts= 36753429
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22137472542751] = TemplatedLoader<0x301, 0x000a2200u, 0x40e16007u, 0x00000001u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22137472542751] = TemplatedLoader<0, 0x000a2200u, 0x40e16007u, 0x00000001u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_C0_1_I8_8888_T0_mtx0_1_I8_flt_T7_mtx1_1_Inv_flt_
// num_verts= 8658216
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21162675840696] = TemplatedLoader<0x301, 0x00022280u, 0x41216009u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21162675840696] = TemplatedLoader<0, 0x00022280u, 0x41216009u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx0_3_I8_flt_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 5573615
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22193439637257] = TemplatedLoader<0x301, 0x000a0200u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22193439637257] = TemplatedLoader<0, 0x000a0200u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx1_3_I16_s16_Nrm_0_0_I16_s8_T0_mtx0_1_I16_s16_T7_mtx1_1_Inv_flt_
// num_verts= 4344828
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21238289947926] = TemplatedLoader<0x301, 0x00030f80u, 0x40e00407u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21238289947926] = TemplatedLoader<0, 0x00030f80u, 0x40e00407u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx0_3_I8_flt_C0_1_I8_8888_
// num_verts= 1418964
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20176780556856] = TemplatedLoader<0x301, 0x00002200u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20176780556856] = TemplatedLoader<0, 0x00002200u, 0x40016009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I8_s16_Nrm_0_0_I8_s8_T0_mtx0_1_I8_s16_T7_mtx1_1_Inv_flt_
// num_verts= 650695
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21066482425878] = TemplatedLoader<0x301, 0x00020a80u, 0x40e00407u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21066482425878] = TemplatedLoader<0, 0x00020a80u, 0x40e00407u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx1_3_I8_s16_C0_1_I8_8888_T0_mtx0_1_I8_s16_T7_mtx1_1_Inv_flt_
// num_verts= 611814
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21083952911382] = TemplatedLoader<0x301, 0x00022280u, 0x40e16007u, 0x80000000u, 0x04800000u>;
	}
	else
#endif
	{
		pvlmap[21083952911382] = TemplatedLoader<0, 0x00022280u, 0x40e16007u, 0x80000000u, 0x04800000u>;
	}
	// P_mtx0_2_Dir_s16_T0_mtx0_1_Dir_s16_T1_mtx0_1_Dir_s16_
// num_verts= 19800
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21271477471492] = TemplatedLoader<0x301, 0x00050100u, 0x40e00006u, 0x00000007u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21271477471492] = TemplatedLoader<0, 0x00050100u, 0x40e00006u, 0x00000007u, 0x00000000u>;
	}
}
