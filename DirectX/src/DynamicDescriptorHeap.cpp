#include "dxpch.h"
#include "DynamicDescriptorHeap.h"
#include "Application.h"
#include "RootSignature.h"
#include "CommandList.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap)
	:	m_descriptorHeapType(type),
		m_numDescriptorsPerHeap(numDescriptorsPerHeap),
		m_descriptorTableBitMask(0),
		m_staleDescriptorTableBitMask(0),
		m_currentCPUDescriptorHandle(D3D12_DEFAULT),
		m_currentGPUDescriptorHandle(D3D12_DEFAULT),
		m_numFreeHandles(0)
{
	m_descriptorHandleIncrementSize = Application::Get()->getDevice()->GetDescriptorHandleIncrementSize(m_descriptorHeapType);

	// Allocate space for staging CPU visible descriptors
	m_descriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_numDescriptorsPerHeap);
}

void DynamicDescriptorHeap::stageDescriptors(uint32_t rootParameterIndex, uint32_t offset, uint32_t numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
	// Cannot stage more than the maximum number of descriptors per heap
	// Cannot stage more than the MaxDescriptorTables root parameters
	if (numDescriptors > m_numDescriptorsPerHeap || rootParameterIndex >= MaxDescriptorTables)
	{
		throw std::bad_alloc();
	}

	DescriptorTableCache& descriptorTableCache = m_descriptorTableCache[rootParameterIndex];

	// Check that the number of descriptors to copy does not exceed the number
	// of descriptors expected in the descriptor table.
	if ((offset + numDescriptors) > descriptorTableCache.numDescriptors)
	{
		throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.baseDescriptor + offset);
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptor, i, m_descriptorHandleIncrementSize);
	}

	// Set the root parameter index bit to make sure the descriptor table
	// at that index is bound to the command list.
	m_staleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

void DynamicDescriptorHeap::commitStagedDescriptors(CommandList& commandlist, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
	// Compute the number of descriptors that need to be copied
	uint32_t numDescriptorsToCommit = computeStaleDescriptorCount();

	if (numDescriptorsToCommit == 0)
	{
		return;
	}

	auto device = Application::Get()->getDevice();
	auto d3dCommandList = commandlist.getCommandList();
	assert(d3dCommandList != nullptr);

	if (m_currentDescriptorHeap == nullptr || m_numFreeHandles < numDescriptorsToCommit)
	{
		m_currentDescriptorHeap = requestDescriptorHeap();
		m_currentCPUDescriptorHandle = m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_currentGPUDescriptorHandle = m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_numFreeHandles = m_numDescriptorsPerHeap;

		commandlist.setDescriptorHeap(m_descriptorHeapType, m_currentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)copied to the new descriptor heap (not just
		// the stale descriptor tables).
		m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
	}

	DWORD rootIndex;
	// Scan from LSB to MSB for a set bit in staleDescriptorsBitMask
	while (_BitScanForward(&rootIndex, m_staleDescriptorTableBitMask))
	{
		UINT numSrcDescriptors = m_descriptorTableCache[rootIndex].numDescriptors;
		D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = m_descriptorTableCache[rootIndex].baseDescriptor;

		D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] =
		{
			m_currentCPUDescriptorHandle
		};
		UINT pDestDescriptorRangeSizes[] =
		{
			numSrcDescriptors
		};

		// Copy the staged CPU visible descriptors to the GPU visible descriptor heap
		device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
			numSrcDescriptors, pSrcDescriptorHandles, nullptr, m_descriptorHeapType);

		// Set the descriptors on the command list using the passed-in setter function
		setFunc(d3dCommandList.Get(), rootIndex, m_currentGPUDescriptorHandle);

		// Offset current CPU and GPU descriptor handles
		m_currentCPUDescriptorHandle.Offset(numSrcDescriptors, m_descriptorHandleIncrementSize);
		m_currentGPUDescriptorHandle.Offset(numSrcDescriptors, m_descriptorHandleIncrementSize);
		m_numFreeHandles -= numSrcDescriptors;

		// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor
		m_staleDescriptorTableBitMask ^= (1 << rootIndex);
	}
}

void DynamicDescriptorHeap::commitStagedDescriptorsForDraw(CommandList& commandlist)
{
	commitStagedDescriptors(commandlist, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::commitStagedDescriptorsForDispatch(CommandList& commandlist)
{
	commitStagedDescriptors(commandlist, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::copyDescriptor(CommandList& commandlist, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
	if (m_currentDescriptorHeap == nullptr || m_numFreeHandles < 1)
	{
		m_currentDescriptorHeap = requestDescriptorHeap();
		m_currentCPUDescriptorHandle = m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_currentGPUDescriptorHandle = m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_numFreeHandles = m_numDescriptorsPerHeap;

		commandlist.setDescriptorHeap(m_descriptorHeapType, m_currentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)copied to the new descriptor heap (not just
		// the stale descriptor tables).
		m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
	}

	auto device = Application::Get()->getDevice();

	D3D12_GPU_DESCRIPTOR_HANDLE hGPU = m_currentGPUDescriptorHandle;
	device->CopyDescriptorsSimple(1, m_currentCPUDescriptorHandle, cpuDescriptor, m_descriptorHeapType);

	m_currentCPUDescriptorHandle.Offset(1, m_descriptorHandleIncrementSize);
	m_currentGPUDescriptorHandle.Offset(1, m_descriptorHandleIncrementSize);
	m_numFreeHandles -= 1;

	return hGPU;
}

void DynamicDescriptorHeap::parseRootSignature(const RootSignature& rootSignature)
{
	// If the root signature changes, all the descriptors muse be (re)bound to the command list
	m_staleDescriptorTableBitMask = 0;

	const auto& rootSignatureDesc = rootSignature.getRootSignatureDesc();

	// Get a bit mask that represents the root parameter that match the descriptor heap tpe for this dynamic descriptor heap
	m_descriptorTableBitMask = rootSignature.getDescriptorTableBitMask(m_descriptorHeapType);
	uint32_t descriptorTableBitMask = m_descriptorTableBitMask;

	uint32_t currentOffset = 0;
	DWORD rootIndex;
	while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters)
	{
		uint32_t numDescriptors = rootSignature.getNumDescriptors(rootIndex);

		DescriptorTableCache& descriptorTableCache = m_descriptorTableCache[rootIndex];
		descriptorTableCache.numDescriptors = numDescriptors;
		descriptorTableCache.baseDescriptor = m_descriptorHandleCache.get() + currentOffset;

		currentOffset += numDescriptors;

		// Flip the descriptor table bit so it's not scanned agina for the current index
		descriptorTableBitMask ^= (1 << rootIndex);

		// Make sure the maximum number of descriptors per descriptor heap has not been exceeded
		assert(currentOffset <= m_numDescriptorsPerHeap && 
			"The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
	}
}

void DynamicDescriptorHeap::reset()
{
	m_availableDescriptorHeaps = m_descriptorHeapPool;
	m_currentDescriptorHeap.Reset();
	m_currentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_currentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_numFreeHandles = 0;
	m_descriptorTableBitMask = 0;
	m_staleDescriptorTableBitMask = 0;

	// Reset the table cache
	for (int i = 0; i < MaxDescriptorTables; ++i)
	{
		m_descriptorTableCache[i].reset();
	}
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::requestDescriptorHeap()
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	if (!m_availableDescriptorHeaps.empty())
	{
		descriptorHeap = m_availableDescriptorHeaps.front();
		m_availableDescriptorHeaps.pop();
	}
	else
	{
		descriptorHeap = createDescriptorHeap();
		m_descriptorHeapPool.push(descriptorHeap);
	}

	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::createDescriptorHeap()
{
	auto device = Application::Get()->getDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = m_descriptorHeapType;
	descriptorHeapDesc.NumDescriptors = m_numDescriptorsPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

uint32_t DynamicDescriptorHeap::computeStaleDescriptorCount() const
{
	uint32_t numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = m_staleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += m_descriptorTableCache[i].numDescriptors;
		staleDescriptorsBitMask ^= (1 << i);
	}

	return numStaleDescriptors;
}
