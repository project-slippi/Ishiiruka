// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "D3DBase.h"
#include "D3DCommandListManager.h"

#include "ShaderConstantsManager.h"

#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

ID3D12Resource* shader_constant_buffers[DX12::SHADER_STAGE_COUNT] = {};
void* shader_constant_buffer_data[SHADER_STAGE_COUNT] = {};
D3D12_GPU_VIRTUAL_ADDRESS shader_constant_buffer_gpu_va[SHADER_STAGE_COUNT] = {};

const unsigned int shader_constant_buffer_sizes[SHADER_STAGE_COUNT] = {
	sizeof(GeometryShaderConstants),
	C_PCONST_END * 4 * sizeof(float),
	sizeof(float) * VertexShaderManager::ConstantBufferSize
};

const unsigned int shader_constant_buffer_padded_sizes[SHADER_STAGE_COUNT] = {
	(shader_constant_buffer_sizes[0] + 0xff) & ~0xff,
	(shader_constant_buffer_sizes[1] + 0xff) & ~0xff,
	(shader_constant_buffer_sizes[2] + 0xff) & ~0xff
};

const unsigned int shader_constant_buffer_slot_count[SHADER_STAGE_COUNT] = {
	50000,
	50000,
	50000
};

const unsigned int shader_constant_buffer_slot_rollover_threshold[SHADER_STAGE_COUNT] = {
	10000,
	10000,
	10000
};

unsigned int shader_constant_buffer_current_slot_index[SHADER_STAGE_COUNT] = {};

void ShaderConstantsManager::Init()
{
	PixelShaderManager::DisableDirtyRegions();
	VertexShaderManager::DisableDirtyRegions();
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		shader_constant_buffer_current_slot_index[i] = 0;

		const unsigned int upload_heap_size = shader_constant_buffer_padded_sizes[i] * shader_constant_buffer_slot_count[i];

		CheckHR(
			D3D::device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(upload_heap_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&shader_constant_buffers[i])
				)
			);

		D3D::SetDebugObjectName12(shader_constant_buffers[i], "constant buffer used to emulate the GX pipeline");

		// Obtain persistent CPU pointer, never needs to be unmapped.
		CheckHR(shader_constant_buffers[i]->Map(0, nullptr, &shader_constant_buffer_data[i]));

		// Obtain GPU VA for upload heap, to avoid repeated calls to ID3D12Resource::GetGPUVirtualAddress.
		shader_constant_buffer_gpu_va[i] = shader_constant_buffers[i]->GetGPUVirtualAddress();
	}
}

void ShaderConstantsManager::Shutdown()
{
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(shader_constant_buffers[i]);

		shader_constant_buffers[i] = nullptr;
		shader_constant_buffer_current_slot_index[i] = 0;
		shader_constant_buffer_data[i] = nullptr;
	}
}

void ShaderConstantsManager::LoadAndSetShaderConstants()
{
	// First, upload new constant buffer data.
	if (GeometryShaderManager::IsDirty())
	{
		shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER]++;

		memcpy(
			static_cast<u8*>(shader_constant_buffer_data[SHADER_STAGE_GEOMETRY_SHADER]) +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_GEOMETRY_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER],
			&GeometryShaderManager::constants,
			shader_constant_buffer_sizes[SHADER_STAGE_GEOMETRY_SHADER]);

		GeometryShaderManager::Clear();

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(GeometryShaderConstants));

		D3D::command_list_mgr->m_dirty_gs_cbv = true;
	}

	if (PixelShaderManager::IsDirty())
	{
		shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER]++;

		memcpy(
			static_cast<u8*>(shader_constant_buffer_data[SHADER_STAGE_PIXEL_SHADER]) +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_PIXEL_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER],
			PixelShaderManager::GetBuffer(),
			shader_constant_buffer_sizes[SHADER_STAGE_PIXEL_SHADER]);

		PixelShaderManager::Clear();

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(PixelShaderConstants));

		D3D::command_list_mgr->m_dirty_ps_cbv = true;
	}

	if (VertexShaderManager::IsDirty())
	{
		shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER]++;

		memcpy(
			static_cast<u8*>(shader_constant_buffer_data[SHADER_STAGE_VERTEX_SHADER]) +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_VERTEX_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER],
			VertexShaderManager::GetBuffer(),
			shader_constant_buffer_sizes[SHADER_STAGE_VERTEX_SHADER]);

		VertexShaderManager::Clear();

		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(VertexShaderConstants));

		D3D::command_list_mgr->m_dirty_vs_cbv = true;
	}

	// Now, actually bind new constants as constant buffers.
	if (D3D::command_list_mgr->m_dirty_gs_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_GS_CBV,
			shader_constant_buffer_gpu_va[SHADER_STAGE_GEOMETRY_SHADER] +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_GEOMETRY_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_GEOMETRY_SHADER]
			);

		D3D::command_list_mgr->m_dirty_gs_cbv = false;
	}

	if (D3D::command_list_mgr->m_dirty_ps_cbv)
	{
		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_PS_CBVONE,
			shader_constant_buffer_gpu_va[SHADER_STAGE_PIXEL_SHADER] +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_PIXEL_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_PIXEL_SHADER]
			);

		D3D::command_list_mgr->m_dirty_ps_cbv = false;
	}

	if (D3D::command_list_mgr->m_dirty_vs_cbv)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS calculated_gpu_va =
			shader_constant_buffer_gpu_va[SHADER_STAGE_VERTEX_SHADER] +
			shader_constant_buffer_padded_sizes[SHADER_STAGE_VERTEX_SHADER] *
			shader_constant_buffer_current_slot_index[SHADER_STAGE_VERTEX_SHADER];

		D3D::current_command_list->SetGraphicsRootConstantBufferView(
			DESCRIPTOR_TABLE_VS_CBV,
			calculated_gpu_va
			);

		if (g_ActiveConfig.bEnablePixelLighting)
			D3D::current_command_list->SetGraphicsRootConstantBufferView(
				DESCRIPTOR_TABLE_PS_CBVTWO,
				calculated_gpu_va
				);

		D3D::command_list_mgr->m_dirty_vs_cbv = false;
	}
}

void ShaderConstantsManager::CheckToResetIndexPositionInUploadHeaps()
{
	for (unsigned int i = 0; i < SHADER_STAGE_COUNT; i++)
	{
		if (shader_constant_buffer_current_slot_index[i] > shader_constant_buffer_slot_rollover_threshold[i])
		{
			shader_constant_buffer_current_slot_index[i] = 0;
		}
	}
}

}