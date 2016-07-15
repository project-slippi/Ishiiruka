// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"

namespace DX12
{

D3DDescriptorHeapManager::D3DDescriptorHeapManager(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12Device* device, unsigned int temporarySlots) :
	m_device(device), m_heap_restart_in_progress(false)
{
	CheckHR(device->CreateDescriptorHeap(desc, IID_PPV_ARGS(m_descriptor_heap.ReleaseAndGetAddressOf())));

	m_descriptor_heap_size = desc->NumDescriptors;
	m_descriptor_increment_size = device->GetDescriptorHandleIncrementSize(desc->Type);
	m_gpu_visible = (desc->Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	if (m_gpu_visible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC cpu_shadow_heap_desc = *desc;
		cpu_shadow_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		CheckHR(device->CreateDescriptorHeap(&cpu_shadow_heap_desc, IID_PPV_ARGS(&m_descriptor_heap_cpu_shadow)));

		m_heap_base_gpu = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
		m_heap_base_gpu_cpu_shadow = m_descriptor_heap_cpu_shadow->GetCPUDescriptorHandleForHeapStart();
	}

	m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

	m_first_temporary_slot_in_heap = m_descriptor_heap_size - temporarySlots;
	m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
}

void D3DDescriptorHeapManager::NotifyHeapRestart()
{
	if (m_heap_restart_in_progress)
	{
		CHECK(false, "Heap restart loop detected. Backend is in a unstable state.");
	}
	else
	{
		// we need to flush all the work here because heap values will be overwrited
		if (!PerfQueryBase::ShouldEmulate())
			D3D::command_list_mgr->ExecuteQueuedWork(true);
		m_heap_restart_in_progress = true;
		// Notify observers that the heap is restarted
		for (auto it : m_heap_restart_callbacks)
			it.second(it.first);
		m_heap_restart_in_progress = false;
	}
}

bool D3DDescriptorHeapManager::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadow, bool temporary)
{
	bool allocated_from_current_heap = true;

	if (!temporary && m_current_permanent_offset_in_heap + 1 >= m_first_temporary_slot_in_heap)
	{
		// If out of room in the heap, start back at beginning.
		allocated_from_current_heap = false;
		m_current_permanent_offset_in_heap = 0;
		NotifyHeapRestart();
	}
	if (temporary)
	{
		DEBUGCHECK(m_first_temporary_slot_in_heap < m_descriptor_heap_size, "Temporary heap allocation on a non-temporary heap.");
	}
	DEBUGCHECK(!gpu_handle || (gpu_handle && m_gpu_visible), "D3D12_GPU_DESCRIPTOR_HANDLE used on non-GPU-visible heap.");

	if (temporary && m_current_temporary_offset_in_heap + 1 >= m_descriptor_heap_size)
	{
		m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
	}

	unsigned int heapOffsetToUse = temporary ? m_current_temporary_offset_in_heap : m_current_permanent_offset_in_heap;

	if (m_gpu_visible)
	{
		gpu_handle->ptr = m_heap_base_gpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

		if (gpu_handle_cpu_shadow)
			gpu_handle_cpu_shadow->ptr = m_heap_base_gpu_cpu_shadow.ptr + heapOffsetToUse * m_descriptor_increment_size;
	}

	cpu_handle->ptr = m_heap_base_cpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

	if (!temporary)
	{
		m_current_permanent_offset_in_heap++;
	}

	return allocated_from_current_heap;
}

bool D3DDescriptorHeapManager::AllocateGroup(D3D12_CPU_DESCRIPTOR_HANDLE* base_cpu_handle, unsigned int num_handles, D3D12_GPU_DESCRIPTOR_HANDLE* base_gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE* base_gpu_handle_cpu_shadow, bool temporary)
{
	bool allocated_from_current_heap = true;

	if (!temporary && m_current_permanent_offset_in_heap + num_handles >= m_first_temporary_slot_in_heap)
	{
		// If out of room in the heap, start back at beginning.
		allocated_from_current_heap = false;
		m_current_permanent_offset_in_heap = 0;
		NotifyHeapRestart();
	}
	if (temporary)
	{
		DEBUGCHECK(m_first_temporary_slot_in_heap < m_descriptor_heap_size, "Temporary heap allocation on a non-temporary heap.");
	}
	DEBUGCHECK(!base_gpu_handle || (base_gpu_handle && m_gpu_visible), "D3D12_GPU_DESCRIPTOR_HANDLE used on non-GPU-visible heap.");

	if (temporary && m_current_temporary_offset_in_heap + num_handles >= m_descriptor_heap_size)
	{
		m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
	}

	unsigned int heapOffsetToUse = temporary ? m_current_temporary_offset_in_heap : m_current_permanent_offset_in_heap;

	if (m_gpu_visible)
	{
		base_gpu_handle->ptr = m_heap_base_gpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

		if (base_gpu_handle_cpu_shadow)
			base_gpu_handle_cpu_shadow->ptr = m_heap_base_gpu_cpu_shadow.ptr + heapOffsetToUse * m_descriptor_increment_size;
	}

	base_cpu_handle->ptr = m_heap_base_cpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

	if (temporary)
	{
		m_current_temporary_offset_in_heap += num_handles;
	}
	else
	{
		m_current_permanent_offset_in_heap += num_handles;
	}

	return allocated_from_current_heap;
}

ID3D12DescriptorHeap* D3DDescriptorHeapManager::GetDescriptorHeap()
{
	return m_descriptor_heap.Get();
}

void D3DDescriptorHeapManager::RegisterHeapRestartCallback(void* owning_object, PFN_HEAP_RESTART_CALLBACK* callback_function)
{
	m_heap_restart_callbacks[owning_object] = callback_function;
}
void D3DDescriptorHeapManager::RemoveHeapRestartCallback(void* owning_object)
{
	m_heap_restart_callbacks.erase(owning_object);
}

}  // namespace DX12