// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/BitSet.h"
#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Common/Intrinsics.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderX64.h"


using namespace Gen;

static const X64Reg src_reg = ABI_PARAM1;
static const X64Reg dst_reg = ABI_PARAM2;
static const X64Reg scratch1 = RAX;
static const X64Reg scratch2 = ABI_PARAM3;
static const X64Reg scratch3 = ABI_PARAM4;
static const X64Reg count_reg = R10;
static const X64Reg skipped_reg = R11;
static const u32 MASKINDEXED = INDEX8 & INDEX16;
static const X64Reg base_reg = RBX;

static const u8* memory_base_ptr = (u8*)&g_main_cp_state.array_strides;

static OpArg MPIC(const void* ptr)
{
	return MDisp(base_reg, PtrOffset(ptr, memory_base_ptr));
}

static __m128 scale_factors[13] = {
	_mm_set_ps1(0.0f),
	_mm_set_ps1(fractionTable[7]),
	_mm_set_ps1(fractionTable[6]),
	_mm_set_ps1(fractionTable[15]),
	_mm_set_ps1(fractionTable[14]),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f),
	_mm_set_ps1(0.0f)
};

VertexLoaderX64::VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att) : VertexLoaderBase(vtx_desc, vtx_att)
{
	if (!IsInitialized())
		return;

	AllocCodeSpace(1024, false);
	ClearCodeSpace();
	GenerateVertexLoader();
	WriteProtect();

	std::string name = GetName();
	JitRegister::Register(region, GetCodePtr(), name.c_str());
}

OpArg VertexLoaderX64::GetVertexAddr(int array, u64 attribute)
{
	OpArg data = MDisp(src_reg, m_src_ofs);
	if (attribute & MASKINDEXED)
	{
		int bits = attribute == INDEX8 ? 8 : 16;
		LoadAndSwap(bits, scratch1, data);
		m_src_ofs += bits / 8;
		if (array == ARRAY_POSITION)
		{
			CMP(bits, R(scratch1), Imm8(-1));
			m_skip_vertex = J_CC(CC_E, true);
		}
		// TODO: Move cached_arraybases into CPState and use MDisp() relative to a constant register loaded with &g_main_cp_state.
		IMUL(32, scratch1, MPIC(&g_main_cp_state.array_strides[array]));
		MOV(64, R(scratch2), MPIC(&cached_arraybases[array]));
		return MRegSum(scratch1, scratch2);
	}
	else
	{
		return data;
	}
}

int VertexLoaderX64::ReadVertex(OpArg data, u64 attribute, int format, int count_in, int count_out, bool dequantize, AttributeFormat* native_format, X64Reg scaling_register)
{
	static const __m128i shuffle_lut[5][3] = {
		{ _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF00L),  // 1x u8
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF01L, 0xFFFFFF00L),  // 2x u8
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFF02L, 0xFFFFFF01L, 0xFFFFFF00L) }, // 3x u8
		{ _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00FFFFFFL),  // 1x s8
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL),  // 2x s8
		_mm_set_epi32(0xFFFFFFFFL, 0x02FFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL) }, // 3x s8
		{ _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0001L),  // 1x u16
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0203L, 0xFFFF0001L),  // 2x u16
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFF0405L, 0xFFFF0203L, 0xFFFF0001L) }, // 3x u16
		{ _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x0001FFFFL),  // 1x s16
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x0203FFFFL, 0x0001FFFFL),  // 2x s16
		_mm_set_epi32(0xFFFFFFFFL, 0x0405FFFFL, 0x0203FFFFL, 0x0001FFFFL) }, // 3x s16
		{ _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00010203L),  // 1x float
		_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L),  // 2x float
		_mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L) }, // 3x float
	};

	X64Reg coords = XMM0;
	int elem_size = 1 << (format / 2);
	int load_bytes = elem_size * count_in;
	OpArg dest = MDisp(dst_reg, m_dst_ofs);

	native_format->components = count_out;
	native_format->enable = true;
	native_format->offset = m_dst_ofs;
	native_format->type = FORMAT_FLOAT;

	m_dst_ofs += sizeof(float) * count_out;

	if (attribute == DIRECT)
		m_src_ofs += load_bytes;

	if (cpu_info.bSSSE3)
	{
		if (load_bytes > 8)
			MOVDQU(coords, data);
		else if (load_bytes > 4)
			MOVQ_xmm(coords, data);
		else
			MOVD_xmm(coords, data);

		PSHUFB(coords, MPIC(&shuffle_lut[format][count_in - 1]));

		// Sign-extend.
		if (format == FORMAT_BYTE)
			PSRAD(coords, 24);
		if (format == FORMAT_SHORT)
			PSRAD(coords, 16);
	}
	else
	{
		// SSE2
		X64Reg temp = XMM1;
		switch (format)
		{
		case FORMAT_UBYTE:
			MOVD_xmm(coords, data);
			PXOR(temp, R(temp));
			PUNPCKLBW(coords, R(temp));
			PUNPCKLWD(coords, R(temp));
			break;
		case FORMAT_BYTE:
			MOVD_xmm(coords, data);
			PUNPCKLBW(coords, R(coords));
			PUNPCKLWD(coords, R(coords));
			PSRAD(coords, 24);
			break;
		case FORMAT_USHORT:
		case FORMAT_SHORT:
			switch (count_in)
			{
			case 1:
				LoadAndSwap(16, scratch3, data);
				MOVD_xmm(coords, R(scratch3)); // .......X
				break;
			case 2:
				LoadAndSwap(32, scratch3, data);
				MOVD_xmm(coords, R(scratch3)); // ......XY
				PSHUFLW(coords, R(coords), 0x24); // .....Y.X
				break;
			case 3:
				LoadAndSwap(64, scratch3, data);
				MOVQ_xmm(coords, R(scratch3)); // ....XYZ.
				PUNPCKLQDQ(coords, R(coords)); // XYZ.XYZ.				
				PSHUFLW(coords, R(coords), 0xAC); // ...Z.Y.X
				break;
			}
			if (format == FORMAT_SHORT)
				PSRAD(coords, 16);
			else
				PSRLD(coords, 16);
			break;
		case FORMAT_FLOAT:
			// Floats don't need to be scaled or converted,
			// so we can just load/swap/store them directly
			// and return early.
			// (In SSSE3 we still need to store them.)
			int i = 0;
			for (; i < count_in; i++)
			{
				LoadAndSwap(32, scratch3, data);
				MOV(32, dest, R(scratch3));
				data.AddMemOffset(sizeof(float));
				dest.AddMemOffset(sizeof(float));
			}
			if (count_out > count_in)
			{
				XOR(32, R(scratch3), R(scratch3));
				for (; i < count_out; i++)
				{
					MOV(32, dest, R(scratch3));
					dest.AddMemOffset(sizeof(float));
				}
			}
			return load_bytes;
		}
		if (count_out > count_in)
		{
			switch (count_in)
			{
			case 1:
			{
				static const __m128i mask = _mm_set_epi32(0, 0, 0, 0xFFFFFFFF);
				PAND(coords, MPIC(&mask));
				break;
			}
			case 2:
			{
				static const __m128i mask = _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF);
				PAND(coords, MPIC(&mask));
				break;
			}
			default:
				break;
			}
		}
	}
	if (format != FORMAT_FLOAT)
	{
		CVTDQ2PS(coords, R(coords));

		if (dequantize)
			MULPS(coords, R(scaling_register));
	}

	switch (count_out)
	{
	case 1: MOVSS(dest, coords); break;
	case 2: MOVLPS(dest, coords); break;
	case 3: MOVUPS(dest, coords); break;
	}

	return load_bytes;
}

void VertexLoaderX64::ReadColor(OpArg data, u64 attribute, int format)
{
	int load_bytes = 0;
	switch (format)
	{
	case FORMAT_24B_888:
	case FORMAT_32B_888x:
	case FORMAT_32B_8888:
		MOV(32, R(scratch1), data);
		if (format != FORMAT_32B_8888)
			OR(32, R(scratch1), Imm32(0xFF000000));
		MOV(32, MDisp(dst_reg, m_dst_ofs), R(scratch1));
		load_bytes = 3 + (format != FORMAT_24B_888);
		break;

	case FORMAT_16B_565:
		//                   RRRRRGGG GGGBBBBB
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
		LoadAndSwap(16, scratch1, data);
		if (cpu_info.bBMI1 && cpu_info.bBMI2)
		{
			MOV(32, R(scratch2), Imm32(0x07C3F7C0));
			PDEP(32, scratch3, scratch1, R(scratch2));

			MOV(32, R(scratch2), Imm32(0xF8FCF800));
			PDEP(32, scratch1, scratch1, R(scratch2));
			ANDN(32, scratch2, scratch2, R(scratch3));

			OR(32, R(scratch1), R(scratch2));
		}
		else
		{
			SHL(32, R(scratch1), Imm8(11));
			LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0));
			LEA(32, scratch3, MScaled(scratch2, SCALE_8, 0));
			AND(32, R(scratch1), Imm32(0x0000F800));
			AND(32, R(scratch2), Imm32(0x00FC0000));
			AND(32, R(scratch3), Imm32(0xF8000000));
			OR(32, R(scratch1), R(scratch2));
			OR(32, R(scratch1), R(scratch3));

			MOV(32, R(scratch2), R(scratch1));
			SHR(32, R(scratch1), Imm8(5));
			AND(32, R(scratch1), Imm32(0x07000700));
			OR(32, R(scratch1), R(scratch2));

			SHR(32, R(scratch2), Imm8(6));
			AND(32, R(scratch2), Imm32(0x00030000));
			OR(32, R(scratch1), R(scratch2));
		}

		OR(32, R(scratch1), Imm32(0x000000FF));
		SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
		load_bytes = 2;
		break;

	case FORMAT_16B_4444:
		//                   RRRRGGGG BBBBAAAA
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
		LoadAndSwap(16, scratch1, data);
		if (cpu_info.bBMI2)
		{
			MOV(32, R(scratch2), Imm32(0x0F0F0F0F));
			PDEP(32, scratch1, scratch1, R(scratch2));
		}
		else
		{
			MOV(32, R(scratch2), R(scratch1));
			SHL(32, R(scratch1), Imm8(8));
			OR(32, R(scratch1), R(scratch2));
			AND(32, R(scratch1), Imm32(0x00FF00FF));

			MOV(32, R(scratch2), R(scratch1));
			SHL(32, R(scratch1), Imm8(4));
			OR(32, R(scratch1), R(scratch2));
			AND(32, R(scratch1), Imm32(0x0F0F0F0F));
		}
		MOV(32, R(scratch2), R(scratch1));
		SHL(32, R(scratch1), Imm8(4));
		OR(32, R(scratch1), R(scratch2));
		SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
		load_bytes = 2;
		break;

	case FORMAT_24B_6666:
		//          RRRRRRGG GGGGBBBB BBAAAAAA
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
		data.AddMemOffset(-1);
		LoadAndSwap(32, scratch1, data);
		if (cpu_info.bBMI2)
		{
			MOV(32, R(scratch2), Imm32(0xFCFCFCFC));
			PDEP(32, scratch1, scratch1, R(scratch2));
			MOV(32, R(scratch2), R(scratch1));
		}
		else
		{
			LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0)); // ______RR RRRRGGGG GGBBBBBB AAAAAA__
			AND(32, R(scratch2), Imm32(0x00003FFC));          // ________ ________ __BBBBBB AAAAAA__
			SHL(32, R(scratch1), Imm8(6));                    // __RRRRRR GGGGGGBB BBBBAAAA AA______
			AND(32, R(scratch1), Imm32(0x3FFC0000));          // __RRRRRR GGGGGG__ ________ ________
			OR(32, R(scratch1), R(scratch2));                 // __RRRRRR GGGGGG__ __BBBBBB AAAAAA__

			LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0)); // RRRRRRGG GGGG____ BBBBBBAA AAAA____
			AND(32, R(scratch2), Imm32(0xFC00FC00));          // RRRRRR__ ________ BBBBBB__ ________
			AND(32, R(scratch1), Imm32(0x00FC00FC));          // ________ GGGGGG__ ________ AAAAAA__
			OR(32, R(scratch1), R(scratch2));                 // RRRRRR__ GGGGGG__ BBBBBB__ AAAAAA__
			MOV(32, R(scratch2), R(scratch1));
		}

		SHR(32, R(scratch1), Imm8(6));
		AND(32, R(scratch1), Imm32(0x03030303));
		OR(32, R(scratch1), R(scratch2));

		SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
		load_bytes = 3;
		break;
	}
	if (attribute == DIRECT)
		m_src_ofs += load_bytes;
}

void VertexLoaderX64::GenerateVertexLoader()
{
	BitSet32 regs = { src_reg, dst_reg, scratch1, scratch2, scratch3, count_reg, skipped_reg, base_reg };
	regs &= ABI_ALL_CALLEE_SAVED;
	ABI_PushRegistersAndAdjustStack(regs, 0);

	// Backup count since we're going to count it down.
	PUSH(32, R(ABI_PARAM3));

	// ABI_PARAM3 is one of the lower registers, so free it for scratch2.
	MOV(32, R(count_reg), R(ABI_PARAM3));

	MOV(64, R(base_reg), R(ABI_PARAM4));
	// Load Contants into registers outside the main loop to reduce memory overhead
	if (m_VtxAttr.PosFormat != FORMAT_FLOAT && m_VtxAttr.ByteDequant)
	{
		MOVAPD(XMM2, MPIC(&scale_factors[0]));
	}
	if (m_VtxDesc.Normal)
	{
		MOVAPD(XMM3, MPIC(&scale_factors[m_VtxAttr.NormalFormat + 1]));
	}

	const u64 tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord,
	};
	const X64Reg treg[8] = {
		XMM4, XMM5, XMM6, XMM7,
		XMM8, XMM9, XMM10, XMM11,
	};

	if (m_VtxAttr.ByteDequant)
	{
		for (int i = 0; i < 8; i++)
		{
			if (tc[i] && m_VtxAttr.texCoord[i].Format != FORMAT_FLOAT)
			{
				MOVAPD(treg[i], MPIC(&scale_factors[5 + i]));
			}
		}
	}

	if (m_VtxDesc.Position & MASKINDEXED)
		XOR(32, R(skipped_reg), R(skipped_reg));

	const u8* loop_start = GetCodePtr();

	if (m_VtxDesc.PosMatIdx)
	{
		m_src_ofs++;
	}

	u32 texmatidx_ofs[8];
	const u64 tm[8] = {
		m_VtxDesc.Tex0MatIdx, m_VtxDesc.Tex1MatIdx, m_VtxDesc.Tex2MatIdx, m_VtxDesc.Tex3MatIdx,
		m_VtxDesc.Tex4MatIdx, m_VtxDesc.Tex5MatIdx, m_VtxDesc.Tex6MatIdx, m_VtxDesc.Tex7MatIdx,
	};
	for (int i = 0; i < 8; i++)
	{
		if (tm[i])
			texmatidx_ofs[i] = m_src_ofs++;
	}

	OpArg data = GetVertexAddr(ARRAY_POSITION, m_VtxDesc.Position);
	ReadVertex(data, m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements + 2, 3,
		m_VtxAttr.ByteDequant, &m_native_vtx_decl.position, XMM2);

	if (m_VtxDesc.Normal)
	{
		for (int i = 0; i < (m_VtxAttr.NormalElements ? 3 : 1); i++)
		{
			if (!i || m_VtxAttr.NormalIndex3)
			{
				data = GetVertexAddr(ARRAY_NORMAL, m_VtxDesc.Normal);
				int elem_size = 1 << (m_VtxAttr.NormalFormat / 2);
				data.AddMemOffset(i * elem_size * 3);
			}
			data.AddMemOffset(ReadVertex(data, m_VtxDesc.Normal, m_VtxAttr.NormalFormat, 3, 3,
				true, &m_native_vtx_decl.normals[i], XMM3));
		}

		m_native_components |= VB_HAS_NRM0;
		if (m_VtxAttr.NormalElements)
			m_native_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	const u64 col[2] = { m_VtxDesc.Color0, m_VtxDesc.Color1 };
	for (int i = 0; i < 2; i++)
	{
		if (col[i])
		{
			data = GetVertexAddr(ARRAY_COLOR + i, col[i]);
			ReadColor(data, col[i], m_VtxAttr.color[i].Comp);
			m_native_components |= VB_HAS_COL0 << i;
			m_native_vtx_decl.colors[i].components = 4;
			m_native_vtx_decl.colors[i].enable = true;
			m_native_vtx_decl.colors[i].offset = m_dst_ofs;
			m_native_vtx_decl.colors[i].type = FORMAT_UBYTE;
			m_dst_ofs += 4;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		int elements = m_VtxAttr.texCoord[i].Elements + 1;
		if (tc[i])
		{
			data = GetVertexAddr(ARRAY_TEXCOORD0 + i, tc[i]);
			ReadVertex(data, tc[i], m_VtxAttr.texCoord[i].Format, elements, tm[i] ? 2 : elements,
				m_VtxAttr.ByteDequant, &m_native_vtx_decl.texcoords[i], treg[i]);
			m_native_components |= VB_HAS_UV0 << i;
		}
		if (tm[i])
		{
			m_native_components |= (VB_HAS_TEXMTXIDX0 | VB_HAS_UV0) << i;
			m_native_vtx_decl.texcoords[i].components = 3;
			m_native_vtx_decl.texcoords[i].enable = true;
			m_native_vtx_decl.texcoords[i].type = FORMAT_FLOAT;
			MOVZX(64, 8, scratch1, MDisp(src_reg, texmatidx_ofs[i]));
			if (tc[i])
			{
				CVTSI2SS(XMM0, R(scratch1));
				MOVSS(MDisp(dst_reg, m_dst_ofs), XMM0);
				m_dst_ofs += sizeof(float);
			}
			else
			{
				m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
				PXOR(XMM0, R(XMM0));
				CVTSI2SS(XMM0, R(scratch1));
				SHUFPS(XMM0, R(XMM0), 0x45); // 000X -> 0X00
				MOVUPS(MDisp(dst_reg, m_dst_ofs), XMM0);
				m_dst_ofs += sizeof(float) * 3;
			}
		}
	}
	if (m_VtxDesc.PosMatIdx)
	{
		MOVZX(32, 8, scratch1, MDisp(src_reg, 0));
	}
	else
	{
		MOV(32, R(scratch1), MPIC(&g_main_cp_state.matrix_index_a));
	}
	AND(32, R(scratch1), Imm8(0x3F));
	MOV(32, MDisp(dst_reg, m_dst_ofs), R(scratch1));
	m_native_vtx_decl.posmtx.components = 4;
	m_native_vtx_decl.posmtx.enable = true;
	m_native_vtx_decl.posmtx.offset = m_dst_ofs;
	m_native_vtx_decl.posmtx.type = FORMAT_UBYTE;
	m_dst_ofs += sizeof(u32);

	// Prepare for the next vertex.

	ADD(64, R(dst_reg), Imm32(m_dst_ofs));
	const u8* cont = GetCodePtr();
	ADD(64, R(src_reg), Imm32(m_src_ofs));

	SUB(32, R(count_reg), Imm8(1));
	J_CC(CC_NZ, loop_start);

	// Get the original count.
	POP(32, R(ABI_RETURN));

	ABI_PopRegistersAndAdjustStack(regs, 0);

	if (m_VtxDesc.Position & MASKINDEXED)
	{
		SUB(32, R(ABI_RETURN), R(skipped_reg));
		RET();

		SetJumpTarget(m_skip_vertex);
		ADD(32, R(skipped_reg), Imm8(1));
		JMP(cont);
	}
	else
	{
		RET();
	}
	m_native_stride = m_dst_ofs;
	m_VertexSize = m_src_ofs;
	m_native_vtx_decl.stride = m_native_stride;
}

bool VertexLoaderX64::EnvironmentIsSupported()
{
	return g_ActiveConfig.iBBoxMode == BBoxGPU || !BoundingBox::active;
}

int VertexLoaderX64::RunVertices(const VertexLoaderParameters &parameters)
{
	const VAT &vat = *parameters.VtxAttr;
	scale_factors[0] = _mm_set_ps1(fractionTable[vat.g0.PosFrac]);
	if (m_native_components & VB_HAS_UVALL)
	{
		scale_factors[5] = _mm_set_ps1(fractionTable[vat.g0.Tex0Frac]);
		scale_factors[6] = _mm_set_ps1(fractionTable[vat.g1.Tex1Frac]);
		scale_factors[7] = _mm_set_ps1(fractionTable[vat.g1.Tex2Frac]);
		scale_factors[8] = _mm_set_ps1(fractionTable[vat.g1.Tex3Frac]);
		scale_factors[9] = _mm_set_ps1(fractionTable[vat.g2.Tex4Frac]);
		scale_factors[10] = _mm_set_ps1(fractionTable[vat.g2.Tex5Frac]);
		scale_factors[11] = _mm_set_ps1(fractionTable[vat.g2.Tex6Frac]);
		scale_factors[12] = _mm_set_ps1(fractionTable[vat.g2.Tex7Frac]);
	}
	m_numLoadedVertices += parameters.count;
	return ((int(*)(const u8* src, u8* dst, int count, const void*))region)(parameters.source, parameters.destination, parameters.count, memory_base_ptr);
}
