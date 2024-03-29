#include "dxpch.h"
#include "DescriptorAllocation.h"
#include "Application.h"
#include "DescriptorAllocatorPage.h"

DescriptorAllocation::DescriptorAllocation()
	:	m_descriptor{0},
		m_numHandles(0),
		m_descriptorSize(0),
		m_page(nullptr)
{}

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t numHandles, uint32_t descriptorSize, std::shared_ptr<DescriptorAllocatorPage> page)
	:	m_descriptor(descriptor),
		m_numHandles(numHandles),
		m_descriptorSize(descriptorSize),
		m_page(page)
{}

DescriptorAllocation::~DescriptorAllocation()
{
	free();
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation)
	:	m_descriptor(allocation.m_descriptor),
		m_numHandles(allocation.m_numHandles),
		m_descriptorSize(allocation.m_descriptorSize),
		m_page(std::move(allocation.m_page))
{
	allocation.m_descriptor.ptr = 0;
	allocation.m_numHandles = 0;
	allocation.m_descriptorSize = 0;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other)
{
	// Free this descriptor if it points to anything
	free();

	m_descriptor = other.m_descriptor;
	m_numHandles = other.m_numHandles;
	m_descriptorSize = other.m_descriptorSize;
	m_page = std::move(other.m_page);

	other.m_descriptor.ptr = 0;
	other.m_numHandles = 0;
	other.m_descriptorSize = 0;

	return *this;
}

bool DescriptorAllocation::isNull() const
{
	return m_descriptor.ptr == 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::getDescriptorHandle(uint32_t offset) const
{
	assert(offset < m_numHandles);
	return { m_descriptor.ptr + (m_descriptorSize * offset) };
}

uint32_t DescriptorAllocation::getNumHandles() const
{
	return m_numHandles;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocation::getDescriptorAllocatorPage() const
{
	return m_page;
}

void DescriptorAllocation::free()
{
	if (isNull() || m_page == nullptr)
	{
		return;
	}

	m_page->free(std::move(*this), Application::Get()->getFrameCount());

	m_descriptor.ptr = 0;
	m_numHandles = 0;
	m_descriptorSize = 0;
	m_page.reset();
}
