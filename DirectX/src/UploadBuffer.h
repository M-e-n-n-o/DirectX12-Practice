#pragma once

#include "Defines.h"

#include <wrl.h>
#include <d3d12.h>

#include <memory>
#include <deque>

/*
*	An UploadBuffer provides a convenient method to upload resources to the GPU.
*	Creates a temporary CPU resource and copies it over to the GPU resource.
*	A single instance of an UploadBuffer can only be associated to a single command list/allocator!
*/
class UploadBuffer
{
public:
	// Use to upload data to the GPU
	struct Allocation
	{
		void* cpu;
		D3D12_GPU_VIRTUAL_ADDRESS gpu;
	};

	/*
	* @param pageSize The size to use to allocate new pages in GPU memory
	*/
	explicit UploadBuffer(size_t pageSize = _2MB);

	size_t getPageSize() const { return m_pageSize; }

	/*
	* Allocate memory in an Upload heap. An allocation must not exceed the size of a page.
	* Use a memcpy or similar method to copy the buffer data to CPU pointer 
	* in the Allocation structure returned from this function.
	*/
	Allocation allocate(size_t sizeInBytes, size_t alignment);

	/*
	* Release all allocated pages. This should only be done when the command list
	* is finished executing on the CommandQueue.
	*/
	void reset();

private:
	// A single page for the allocator
	struct Page
	{
		Page(size_t sizeInBytes);
		~Page();

		// Check to see if the page has room to satify the requested allocation.
		bool hasSpace(size_t sizeInBytes, size_t alignment) const;

		// Allocate memory from the page. Throws std::bad_alloc if the allocation
		// size is larger that than the page size or the size of the allocation
		// exceeds the remaining space in the page.
		Allocation allocate(size_t sizeInBytes, size_t alignment);

		// Reset the page for reuse
		void reset();

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;

		// Base pointer
		void* m_cpuPtr;
		D3D12_GPU_VIRTUAL_ADDRESS m_gpuPtr;

		// Allocated page size
		size_t m_pageSize;
		// Current allocation offset in bytes
		size_t m_offset;
	};

	// A pool of memory pages
	using PagePool = std::deque<std::shared_ptr<Page>>;

	// Request a page from the pool of available pages
	// or create a new page if there are no available pages.
	std::shared_ptr<Page> requestPage();

	PagePool m_pagePool;
	PagePool m_availablePages;

	std::shared_ptr<Page> m_currentPage;

	// The size of each page of memory
	size_t m_pageSize;
};