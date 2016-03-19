// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include <unordered_map>

#include "VideoBackends/D3D12/D3DState.h"


namespace DX12
{

// This class provides an abstraction for D3D12 descriptor heaps.
class D3DDescriptorHeapManager
{
public:

	D3DDescriptorHeapManager(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12Device* device, unsigned int temporarySlots = 0);

	bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadow = nullptr, bool temporary = false);
	bool AllocateGroup(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, unsigned int num_handles, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadows = nullptr, bool temporary = false);

	ID3D12DescriptorHeap* GetDescriptorHeap();

	// Allow other components to register for a callback each time a Heap starts over
	using PFN_HEAP_RESTART_CALLBACK = void(void* owning_object);
	void RegisterHeapRestartCallback(void* owning_object, PFN_HEAP_RESTART_CALLBACK* callback_function);
	void RemoveHeapRestartCallback(void* owning_object);

private:

	ID3D12Device* m_device = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_descriptor_heap;
	ComPtr<ID3D12DescriptorHeap> m_descriptor_heap_cpu_shadow;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_cpu{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_heap_base_gpu{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_gpu_cpu_shadow{};

	unsigned int m_current_temporary_offset_in_heap{};
	unsigned int m_current_permanent_offset_in_heap{};

	unsigned int m_descriptor_increment_size{};
	unsigned int m_descriptor_heap_size{};
	bool m_gpu_visible{};

	unsigned int m_first_temporary_slot_in_heap{};
	bool m_heap_restart_in_progress{};
	std::map<void*, PFN_HEAP_RESTART_CALLBACK*> m_heap_restart_callbacks;
	void NotifyHeapRestart();
};

}  // namespace