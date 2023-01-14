#pragma once

#include "d3dx12.h"

#include <wrl.h>

#include <cstdint>
#include <memory>
#include <queue>
#include <functional>

class CommandList;
class RootSignature;

/*
* Class which handles copying CPU visible descriptors to GPU visible descriptor heaps and actually binding them to the command list.
*/
class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap = 1024);
	virtual ~DynamicDescriptorHeap() = default;

	/*
	* Stages a contiguous range of CPU visible descriptors.
	* Descriptors are not copied to the GPU visible descriptor heap until
	* the commitStagedDescriptors function is called
	*/
	void stageDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor);

	/*
	* Copy all of the staged descriptors to the GPU visible descriptor heap and
    * bind the descriptor heap and the descriptor tables to the command list.
	* The passed-in function object is used to set the GPU visible descriptors
    * on the command list. Two possible functions are:
    *   * Before a draw    : ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
    *   * Before a dispatch: ID3D12GraphicsCommandList::SetComputeRootDescriptorTable
    * 
    * Since the DynamicDescriptorHeap can't know which function will be used, it must
    * be passed as an argument to the function.
	*/
	void commitStagedDescriptors(CommandList& commandlist, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);
	void commitStagedDescriptorsForDraw(CommandList& commandlist);
	void commitStagedDescriptorsForDispatch(CommandList& commandlist);

	/*
	* Copies a single CPU visible descriptor to a GPU visible descriptor heap.
    * This is useful for the
    *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat
    *   * ID3D12GraphicsCommandList::ClearUnorderedAccessViewUint
    * methods which require both a CPU and GPU visible descriptors for a UAV 
    * resource.
    * 
    * @param commandList The command list is required in case the GPU visible
    * descriptor heap needs to be updated on the command list.
    * @param cpuDescriptor The CPU descriptor to copy into a GPU visible 
    * descriptor heap.
    * 
    * @return The GPU visible descriptor.
	*/
    D3D12_GPU_DESCRIPTOR_HANDLE copyDescriptor(CommandList& commandlist, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

    /*
    * Parse the root signature to determine which root parameters contain
    * descriptor tables and determine the number of descriptors needed for each table.
    */
    void parseRootSignature(const RootSignature& rootSignature);

    /*
    * Reset used descriptors. This should only be done if any descriptors that are
    * being referenced by a command list has finished executing on the command queue.
    */
    void reset();

private:
    // Request a descriptor heap if one is available
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> requestDescriptorHeap();
    // Create a new descriptor heap if no descriptor heap is available
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap();

    // Compute the number of stale descriptors that need to be copied
    // to GPU visible descriptor heap
    uint32_t computeStaleDescriptorCount() const;

    /*
    * The maximum number of descriptor tables per root signature.
    * A 32-bit mask is used to keep track of the root parameter indices
    * that are descriptor tables.
    */
    static const uint32_t MaxDescriptorTables = 32;

    /*
    * A structure that represents a descriptor table enty in the root signature
    */
    struct DescriptorTableCache
    {
        DescriptorTableCache()
            :   numDescriptors(0),
                baseDescriptor(nullptr)
        {}

        // Reset the table cache
        void reset()
        {
            numDescriptors = 0;
            baseDescriptor = nullptr;
        }

        // The number of descriptors in this descriptor table
        uint32_t numDescriptors;
        // The pointer to the descriptor in the descriptor handle cache
        D3D12_CPU_DESCRIPTOR_HANDLE* baseDescriptor;
    };


    // Describes the type of descriptors that can be staged using this 
    // dynamic descriptor heap.
    // Valid values are:
    //   * D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    //   * D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    // This parameter also determines the type of GPU visible descriptor heap to 
    // create.
    D3D12_DESCRIPTOR_HEAP_TYPE m_descriptorHeapType;

    // The number of descriptors to allocate in new GPU visible descriptor heaps
    uint32_t m_numDescriptorsPerHeap;

    // The increment size of a descriptor
    uint32_t m_descriptorHandleIncrementSize;

    // The descriptor handle cache
    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_descriptorHandleCache;

    // Descriptor handle cache per descriptor table
    DescriptorTableCache m_descriptorTableCache[MaxDescriptorTables];

    // Each bit in the bit mask represents the index in the root signature
    // that contains a descriptor table.
    uint32_t m_descriptorTableBitMask;
    // Each bit set in the bit mask represents a descriptor table in the
    // root signature that has changed since the last time the descriptors were copied
    uint32_t m_staleDescriptorTableBitMask;

    using DescriptorHeapPool = std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>;

    DescriptorHeapPool m_descriptorHeapPool;
    DescriptorHeapPool m_availableDescriptorHeaps;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_currentDescriptorHeap;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_currentGPUDescriptorHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_currentCPUDescriptorHandle;

    uint32_t m_numFreeHandles;
};