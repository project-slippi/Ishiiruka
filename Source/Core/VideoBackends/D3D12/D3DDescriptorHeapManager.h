// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include <memory>
#include <unordered_map>

#include "Common/AllocationMap.h"
#include "VideoBackends/D3D12/D3DState.h"

namespace DX12
{

// Allow other components to register for a callback each time a Heap starts over
using PFN_HEAP_RESTART_CALLBACK = void(void* owning_object);
// This class provides an abstraction for D3D12 descriptor heaps.
class D3DDescriptorHeapManager
{
public:
	D3DDescriptorHeapManager(ID3D12DescriptorHeap* descriptor_heap,
		ID3D12DescriptorHeap* shadow_descriptor_heap,
		size_t num_descriptors,
		size_t descriptor_increment_size,
		size_t temporary_slots);
	~D3DDescriptorHeapManager();

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptor_heap; }

	bool Allocate(size_t* out_index,
		D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
		D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle_shadow,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);

	void Free(size_t index);

	bool AllocateTemporary(size_t num_handles,
		D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_base_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_base_handle);

	static std::unique_ptr<D3DDescriptorHeapManager> Create(ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		D3D12_DESCRIPTOR_HEAP_FLAGS flags,
		size_t num_descriptors,
		size_t temporary_slots = 0);

private:
	static void QueueFenceCallback(void* owner, u64 fence_value);

	bool TryAllocateTemporaryHandles(size_t num_handles,
		D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_base_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_base_handle);

	ID3D12DescriptorHeap* m_descriptor_heap;
	ID3D12DescriptorHeap* m_shadow_descriptor_heap;
	size_t m_num_descriptors;
	size_t m_descriptor_increment_size;
	size_t m_temporary_slots;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_cpu = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_shadow_heap_base = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_heap_base_gpu = {};

	Common::AllocationMap m_slots;

	size_t m_current_temporary_descriptor_index = 0;
	size_t m_gpu_temporary_descriptor_index = 0;

	// Fences tracking GPU index
	std::deque<std::pair<u64, size_t>> m_fences;
	ID3D12Fence* m_fence = nullptr;
};

class D3DSamplerHeapManager
{
	static constexpr size_t HEAP_COUNT = 4;
public:
	D3DSamplerHeapManager(std::array<ID3D12DescriptorHeap*, HEAP_COUNT>& descriptor_heap,
		size_t num_descriptors,
		size_t descriptor_increment_size);
	~D3DSamplerHeapManager();

	bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle);

	bool AllocateGroup(unsigned int num_handles,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles);

	ID3D12DescriptorHeap* GetDescriptorHeap() const;

	static std::unique_ptr<D3DSamplerHeapManager> Create(ID3D12Device* device,
		size_t num_descriptors);

	void RegisterHeapRestartCallback(void* owning_object, PFN_HEAP_RESTART_CALLBACK* callback_function);
	void RemoveHeapRestartCallback(void* owning_object);
	
	struct SamplerStateSet
	{
		SamplerState desc0;
		SamplerState desc1;
		SamplerState desc2;
		SamplerState desc3;
		SamplerState desc4;
		SamplerState desc5;
		SamplerState desc6;
		SamplerState desc7;
	};

	D3D12_GPU_DESCRIPTOR_HANDLE GetHandleForSamplerGroup(SamplerState* sampler_state, unsigned int num_sampler_samples);

private:
	
	std::array<ID3D12DescriptorHeap*, HEAP_COUNT> m_descriptor_heaps;
	size_t m_descriptor_heap_index;
	size_t m_descriptor_heap_size;
	size_t m_descriptor_increment_size;
	size_t m_current_offset = 0;

	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, HEAP_COUNT> m_heap_bases_cpu;
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, HEAP_COUNT> m_heap_bases_gpu;
	bool m_heap_restart_in_progress{};
	std::map<void*, PFN_HEAP_RESTART_CALLBACK*> m_heap_restart_callbacks;
	void NotifyHeapRestart();

	struct hash_sampler_desc
	{
		size_t operator()(const SamplerStateSet sampler_state_set) const
		{
			return sampler_state_set.desc0.hex;
		}
	};

	std::unordered_map<SamplerStateSet, D3D12_GPU_DESCRIPTOR_HANDLE, hash_sampler_desc> m_sampler_map;
};

}  // namespace