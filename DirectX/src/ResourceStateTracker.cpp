#include "dxpch.h"
#include "ResourceStateTracker.h"
#include "CommandList.h"

// Static definitions
std::mutex ResourceStateTracker::s_globalMutex;
bool ResourceStateTracker::s_isLocked = false;
ResourceStateTracker::ResourceStateMap ResourceStateTracker::s_globalResourceStates;

void ResourceStateTracker::resourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
	if (barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		// Just push non-transition barriers to the resource barriers array.
		m_resourceBarriers.push_back(barrier);
		return;
	}

	const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

	// First check if there is already a known "final" state for the given resource.
	// If there is, the resource has been used on the command list before and
	// already has a known state within the command list execution.
	const auto itr = m_finalResourceStates.find(transitionBarrier.pResource);
	if (itr != m_finalResourceStates.end())
	{
		auto& resourceState = itr->second;
		// If the known final state of the resource is different...
		if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
			!resourceState.subresourceStates.empty())
		{
			// First transition all of the subresources if they are different than the StateAfter.
			for (auto subresourceState : resourceState.subresourceStates)
			{
				if (transitionBarrier.StateAfter != subresourceState.second)
				{
					D3D12_RESOURCE_BARRIER newBarrier = barrier;
					newBarrier.Transition.Subresource = subresourceState.first;
					newBarrier.Transition.StateBefore = subresourceState.second;
					m_resourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			auto finalState = resourceState.getSubresourceState(transitionBarrier.Subresource);
			if (transitionBarrier.StateAfter != finalState)
			{
				// Push a new transition barrier with the correct before state.
				D3D12_RESOURCE_BARRIER newBarrier = barrier;
				newBarrier.Transition.StateBefore = finalState;
				m_resourceBarriers.push_back(newBarrier);
			}
		}
	}
	else // In this case, the resource is being used on the command list for the first time.
	{
		// Add a pending barrier. The pending barriers will be resolved
		// before the command list is executed on the command queue.
		m_pendingResourceBarriers.push_back(barrier);
	}

	// Push the final known state (possibly replacing the previously known state for the subresource).
	m_finalResourceStates[transitionBarrier.pResource].setSubresourceState(transitionBarrier.Subresource, transitionBarrier.StateAfter);
}

void ResourceStateTracker::transitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, UINT subResource)
{
	if (resource != nullptr)
	{
		resourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource));
	}
}

void ResourceStateTracker::uavBarrier(ID3D12Resource* resource)
{
	resourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(resource));
}

void ResourceStateTracker::aliasBarrier(ID3D12Resource* beforeResource, ID3D12Resource* afterResource)
{
	resourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(beforeResource, afterResource));
}

uint32_t ResourceStateTracker::flushPendingResourceBarriers(CommandList& commandList)
{
	assert(s_isLocked);

	// Resolve the pending resource barriers by checking the global state of the
	// (sub)resources. Add barriers if the pending state and the global state do not match.
	ResourceBarriers resourceBarriers;

	// Reserve enough space (worst-case, all pending barriers)
	resourceBarriers.reserve(m_pendingResourceBarriers.size());

	for (auto pendingBarrier : m_pendingResourceBarriers)
	{
		// Only transition barries should be pending
		if (pendingBarrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
		{
			continue;
		}

		auto pendingTransition = pendingBarrier.Transition;
		const auto& itr = s_globalResourceStates.find(pendingTransition.pResource);
		if (itr != s_globalResourceStates.end())
		{
			// If all subresources are being transitioned, and there are multiple
			// subresources of the resource that are in a different state...
			auto& resourceState = itr->second;
			if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
				!resourceState.subresourceStates.empty())
			{
				// Transition all subresources
				for (auto subresourceState : resourceState.subresourceStates)
				{
					if (pendingTransition.StateAfter != subresourceState.second)
					{
						D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
						newBarrier.Transition.Subresource = subresourceState.first;
						newBarrier.Transition.StateBefore = subresourceState.second;
						resourceBarriers.push_back(newBarrier);
					}
				}
			}
			else
			{
				// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
				auto globalState = (itr->second).getSubresourceState(pendingTransition.Subresource);
				if (pendingTransition.StateAfter != globalState)
				{
					// Fix-up the before state based on current global state of the resource.
					pendingBarrier.Transition.StateBefore = globalState;
					resourceBarriers.push_back(pendingBarrier);
				}
			}
		}
	}

	UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3dCommandList = commandList.getCommandList();
		d3dCommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	m_pendingResourceBarriers.clear();

	return numBarriers;
}

void ResourceStateTracker::flushResourceBarriers(CommandList& commandList)
{
	UINT numBarriers = static_cast<UINT>(m_resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3dCommandList = commandList.getCommandList();
		d3dCommandList->ResourceBarrier(numBarriers, m_resourceBarriers.data());
		m_resourceBarriers.clear();
	}
}

void ResourceStateTracker::commitFinalResourceStates()
{
	assert(s_isLocked);

	// Commit final resource states to the global resource state array (map).
	for (const auto& resourceState : m_finalResourceStates)
	{
		s_globalResourceStates[resourceState.first] = resourceState.second;
	}

	m_finalResourceStates.clear();
}

void ResourceStateTracker::reset()
{
	// Reset the pending, current, and final resource states.
	m_pendingResourceBarriers.clear();
	m_resourceBarriers.clear();
	m_finalResourceStates.clear();
}

void ResourceStateTracker::Lock()
{
	s_globalMutex.lock();
	s_isLocked = true;
}

void ResourceStateTracker::Unlock()
{
	s_isLocked = false;
	s_globalMutex.unlock();
}

void ResourceStateTracker::AddGlobalResourceStates(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(s_globalMutex);
		s_globalResourceStates[resource].setSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}

void ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(s_globalMutex);
		s_globalResourceStates.erase(resource);
	}
}
