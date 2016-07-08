// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/G_GNUEDA_pvt.h"
#include "VideoCommon/VertexLoader_Template.h"



void G_GNUEDA_pvt::Initialize(std::map<u64, TCompiledLoaderFunction> &pvlmap)
{
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_flt_
// num_verts= 16423699
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[21316665904952] = TemplatedLoader<0x301, 0x00030f00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[21316665904952] = TemplatedLoader<0, 0x00030f00u, 0x41201009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_C0_1_I16_8888_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
// num_verts= 3148318
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23069966952457] = TemplatedLoader<0x301, 0x000f3300u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23069966952457] = TemplatedLoader<0, 0x000f3300u, 0x41216009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_T0_mtx0_1_I8_flt_T1_mtx0_1_I8_flt_
// num_verts= 3032380
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[22194097903625] = TemplatedLoader<0x301, 0x000a0300u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[22194097903625] = TemplatedLoader<0, 0x000a0300u, 0x41200009u, 0x00000009u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_T0_mtx0_1_I8_flt_
// num_verts= 1457768
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20845968380728] = TemplatedLoader<0x301, 0x00020300u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20845968380728] = TemplatedLoader<0, 0x00020300u, 0x41200009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_
// num_verts= 1414283
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20456864321336] = TemplatedLoader<0x301, 0x00000f00u, 0x40001009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20456864321336] = TemplatedLoader<0, 0x00000f00u, 0x40001009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s16_C0_1_Dir_8888_T0_mtx0_1_Dir_flt_
// num_verts= 254612
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20688359175493] = TemplatedLoader<0x301, 0x00011100u, 0x41216006u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20688359175493] = TemplatedLoader<0, 0x00011100u, 0x41216006u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_s8_C0_1_Dir_8888_T0_mtx0_1_Dir_u8_
// num_verts= 145692
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20373467533313] = TemplatedLoader<0x301, 0x00011100u, 0x40216002u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20373467533313] = TemplatedLoader<0, 0x00011100u, 0x40216002u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx1_3_I16_flt_Nrm_0_0_I16_flt_T0_mtx0_1_I16_flt_T1_mtx0_1_I16_flt_
// num_verts= 59267
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[23338860188681] = TemplatedLoader<0x301, 0x000f0f00u, 0x41201009u, 0x80000009u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[23338860188681] = TemplatedLoader<0, 0x000f0f00u, 0x41201009u, 0x80000009u, 0x00000000u>;
	}
	// P_mtx1_3_I8_flt_C0_1_I8_8888_
// num_verts= 23726
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20470985816632] = TemplatedLoader<0x301, 0x00002200u, 0x40016009u, 0x80000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20470985816632] = TemplatedLoader<0, 0x00002200u, 0x40016009u, 0x80000000u, 0x00000000u>;
	}
	// P_mtx0_2_Dir_u8_
// num_verts= 9896
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20153366285663] = TemplatedLoader<0x301, 0x00000100u, 0x40000000u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20153366285663] = TemplatedLoader<0, 0x00000100u, 0x40000000u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_I16_flt_Nrm_0_0_I8_flt_
// num_verts= 9737
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20160025996088] = TemplatedLoader<0x301, 0x00000b00u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20160025996088] = TemplatedLoader<0, 0x00000b00u, 0x40001009u, 0x00000000u, 0x00000000u>;
	}
	// P_mtx0_3_Dir_flt_C0_1_Dir_8888_T0_mtx0_1_I8_u8_
// num_verts= 5576
#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3)
	{
		pvlmap[20541983854904] = TemplatedLoader<0x301, 0x00021100u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
	else
#endif
	{
		pvlmap[20541983854904] = TemplatedLoader<0, 0x00021100u, 0x40216009u, 0x00000000u, 0x00000000u>;
	}
}
