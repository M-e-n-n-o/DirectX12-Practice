//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTriangle.h"
#include "DXSampleHelper.h"
#include "Win32Application.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/DXRHelper.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include <stdexcept>
#include <windowsx.h>

D3D12HelloTriangle::D3D12HelloTriangle(const UINT width, const UINT height, const std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0)
{
}

void D3D12HelloTriangle::OnInit()
{
	LoadPipeline();
	LoadAssets();

	// Check the raytracing capabilites of the device
	CheckRaytracingSupport();

	// Setup the acceleration structures (AS) for raytracing. When setting up
	// geometry, each bottom-level AS has its own transform matrix.
	CreateAccelerationStructures();

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	ThrowIfFailed(m_commandList->Close());

	// Create the raytracing pipeline, associating the shader code to symbol names
	// and to their root signatures, and defining the amount of memory carried by
	// rays (ray payload).
	CreateRaytracingPipeline();


	// Create a constant buffer, with a color for each vertex of the triangle, for
	// each triangle instance.
	//CreateGlobalConstantBuffer();
	CreatePerInstanceConstantBuffers();

	// Allocate the buffer storing the raytracing output, with the same dimensions
	// as the target image.
	CreateRaytracingOutputBuffer();

	// Create a buffer to store the modelview and perspective camera matrices.
	CreateCameraBuffer();

	// Create the buffer containing the raytracing result (always output in a
	// UAV), and create the heap referencing the resources used by the raytracing,
	// such as the acceleration structure.
	CreateShaderResourceHeap();

	// Create the shader binding table and indicating which shaders
	// are invoked for each instance in the AS.
	CreateShaderBindingTable();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS(&m_device)
			));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS(&m_device)
			));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
		));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}

		CreateDepthBuffer();
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
	// Create an empty root signature.
	{
		// The root signature describes which data is accessed by the shader. The camera matrices
		// are held in a constant buffer, itself referenced the heap. To do this we reference a
		// range in the heap, and use that range as the sole parameter of the shader. The camera
		// buffer is associated in the index 0, making it accessible in the shader in the b0 register.
		CD3DX12_ROOT_PARAMETER constantParameter;
		CD3DX12_DESCRIPTOR_RANGE range;
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		constantParameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(1, &constantParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		const std::wstring shader = L"shaders/shaders.hlsl";
		
		ThrowIfFailed(D3DCompileFromFile(shader.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(shader.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	// Create the vertex buffer.
	{
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;

		// Create a vertex buffer for a ground plane
		CreatePlaneVB();
	}

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
	}
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate()
{
	UpdateCameraBuffer();
}

// Render the scene.
void D3D12HelloTriangle::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::OnResize()
{
	//for (uint32_t i = 0; i < FrameCount; ++i)
	//{
	//	// Any references to the back buffers must be released
	//	// before the swap chain can be resized.
	//	m_renderTargets[i].Reset();
	//}

	//DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	//ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
	//ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height,
	//	swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

	//auto rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	//for (uint32_t i = 0; i < FrameCount; ++i)
	//{
	//	ComPtr<ID3D12Resource> backBuffer;
	//	ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

	//	m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

	//	m_renderTargets[i] = backBuffer;

	//	rtvHandle.Offset(rtvDescriptorSize);
	//}
}

void D3D12HelloTriangle::OnKeyUp(UINT8 key)
{
	if (key == VK_SPACE)
	{
		m_raster = !m_raster;
		_RPT1(0, "Using raytracer: %d\n", !m_raster);
	}
}

void D3D12HelloTriangle::OnKeyDown(UINT8 key)
{
	DirectX::XMFLOAT3 movement(0.0f, 0.0f, 0.0f);

	if (key == 'W') movement.z = CAMERA_SPEED;
	if (key == 'S') movement.z = -CAMERA_SPEED;
	if (key == 'D') movement.x = CAMERA_SPEED;
	if (key == 'A') movement.x = -CAMERA_SPEED;
	if (key == VK_SPACE) movement.y = CAMERA_SPEED;
	if (key == VK_SHIFT) movement.y = -CAMERA_SPEED;

	float dx = movement.z * cos(DirectX::XMConvertToRadians(m_cameraRotation.y - 90));
	float dz = movement.z * sin(DirectX::XMConvertToRadians(m_cameraRotation.y - 90));
	dx += movement.x * cos(DirectX::XMConvertToRadians(m_cameraRotation.y));
	dz += movement.x * sin(DirectX::XMConvertToRadians(m_cameraRotation.y));

	m_cameraPosition.x += dx;
	m_cameraPosition.z += dz;
	m_cameraPosition.y += movement.y;
}

void D3D12HelloTriangle::OnMouseMove(UINT8 button, UINT32 movement)
{
	static float prevPosX = 0;
	static float prevPosY = 0;

	float currentPosX = GET_Y_LPARAM(movement);
	float currentPosY = GET_X_LPARAM(movement);

	m_cameraRotation.x += (currentPosX - prevPosX) * 0.1f;
	m_cameraRotation.y += (currentPosY - prevPosY) * 0.1f;

	prevPosX = currentPosX;
	prevPosY = currentPosY;
}

void D3D12HelloTriangle::PopulateCommandList() const
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	if (m_raster)
	{
		std::vector<ID3D12DescriptorHeap*> heaps = { m_constHeap.Get() };
		m_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
		// Set the root descriptor table 0 to the constant buffer descriptor heap
		m_commandList->SetGraphicsRootDescriptorTable(0, m_constHeap->GetGPUDescriptorHandleForHeapStart());

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Draw triangle
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->DrawInstanced(3, 1, 0, 0);

		// Draw plane
		m_commandList->IASetVertexBuffers(0, 1, &m_planeBufferView);
		m_commandList->DrawInstanced(6, 1, 0, 0);
	}
	else
	{
		// Bind the descriptor heap giving access to the top-level acceleration
		// structure, as well as the raytracing output.
		std::vector<ID3D12DescriptorHeap*> heaps = { m_srcUavHeap.Get() };
		m_commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

		// On the last frame, the raytracing output was used as a copy source, to
		// copy its contents into the render target. Now we need to transition it to
		// a UAV so that the shaders can write in it.
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_commandList->ResourceBarrier(1, &transition);

		// Setup the raytracing task
		D3D12_DISPATCH_RAYS_DESC desc{};

		// The layout of the SBT is as follows: ray generation shader, miss
		// shaders, hit groups. As described in the CreateShderBindingTable method,
		// all SBT entries of a given type have the same size to allow a fixed stride.
		// The ray generation shaders are always at the beginning of the SBT.
		uint32_t rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize();
		desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
		desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

		// The miss shaders are in the second SBT section, right after the ray
		// generation shader. We have one miss shader for the camera rays and one
		// for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
		// also indicate the stride between the two miss shaders, which is the size
		// of a SBT entry.
		uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
		desc.MissShaderTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
		desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
		desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

		// The hit groups section start after the miss shaders. In this sample we
		// have 1 hit group for the traingle.
		uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
		desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
		desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
		desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

		// Dimensions of the image to render, identical to a kernel launch dimension.
		// This also defines the number of threads running the ray generation program.
		desc.Width = GetWidth();
		desc.Height = GetHeight();
		desc.Depth = 1;

		// Bind the raytracing pipeline
		m_commandList->SetPipelineState1(m_rtStateObject.Get());
		// Dispatch the rays and write to the raytracing output
		m_commandList->DispatchRays(&desc);

		// The raytracing output needs to be copied to the actual render target used
		// for display. For this, we need to transition the raytracing output from a
		// UAV to a copy source, and the render target buffer to a copy destination.
		// We can then do the actual copy, before transitioning the render target
		// buffer into a render target, that will then be used to display the image.
		transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_commandList->ResourceBarrier(1, &transition);
		transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
		m_commandList->ResourceBarrier(1, &transition);
		// Copy the actual buffers
		m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(), m_outputResource.Get());
		// Transition back
		transition = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &transition);
	}

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12HelloTriangle::CheckRaytracingSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
	ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
	{
		throw std::runtime_error("Raytracing not supported on device");
	}
}

void D3D12HelloTriangle::CreateAccelerationStructures()
{
	// Combine the BLAS and TLAS builds to construct the entire acceleration
	// structure required to raytrace the scene.

	// Build the bottom-level AS
	AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS({ {m_vertexBuffer.Get(), 3} });
	AccelerationStructureBuffers planeBottomLevelBuffers = CreateBottomLevelAS({ { m_planeBuffer.Get(), 6 } });

	m_instances = { 
		// Triangles
		{bottomLevelBuffers.pResult, DirectX::XMMatrixIdentity()}, 
		{bottomLevelBuffers.pResult, DirectX::XMMatrixTranslation(-0.6f, 0, 0)},
		{bottomLevelBuffers.pResult, DirectX::XMMatrixTranslation(0.6f, 0, 0)},

		// Plane
		{planeBottomLevelBuffers.pResult, DirectX::XMMatrixTranslation(0, 0, 0)}
	};
	// Build the top-level AS's
	CreateTopLevelAS(m_instances);

	// Flush the command list and wait for it to finish
	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	m_fenceValue++;
	m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	WaitForSingleObject(m_fenceEvent, INFINITE);

	// Once the command list is finished executing, reset it to be reused for rendering
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Store the AS buffers. The rest of the buffers will be released once we exit the function
	m_bottomLevelAS = bottomLevelBuffers.pResult;
}

D3D12HelloTriangle::AccelerationStructureBuffers D3D12HelloTriangle::CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers)
{
	// Create a bottom-level acceleration structure based on a list of vertex
	// buffers in GPU memory along with their vertex count. The build is then done
	// in 3 steps: gathering the geometry, computing the sizes of the required
	// buffers, and building the actual AS.

	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

	// Adding all vertex buffers and not transforming their position
	for (const auto& buffer : vVertexBuffers)
	{
		bottomLevelAS.AddVertexBuffer(buffer.first.Get(), 0, buffer.second, sizeof(Vertex), 0, 0);
	}

	// The AS build requires some scratch space to store temporary information.
	// The amount of scratch memory is dependent on the scene complexity.
	UINT64 scratchSizeInBytes = 0;

	// The final AS also needs to be stored in addition to the existing vertex buffers.
	// It size is also dependent on the scene complexity.
	UINT64 resultSizeInBytes = 0;

	bottomLevelAS.ComputeASBufferSizes(m_device.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

	// Once the sizes are obtained, the application is responsible for allocating
	// the necessary buffers. Since the entire generation will be done on the GPU,
	// we can directly allocate those on the default heap
	AccelerationStructureBuffers buffers;

	buffers.pStratch = nv_helpers_dx12::CreateBuffer(m_device.Get(), scratchSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);

	buffers.pResult = nv_helpers_dx12::CreateBuffer(m_device.Get(), resultSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nv_helpers_dx12::kDefaultHeapProps);

	// Build the acceleration structure. Note that this call integrates a barrier
	// on the generated AS, so that it can be used to compute a top-level AS right
	// after this method.
	bottomLevelAS.Generate(m_commandList.Get(), buffers.pStratch.Get(), buffers.pResult.Get(), false, nullptr);

	return buffers;
}

void D3D12HelloTriangle::CreateTopLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances)
{
	// Create the main acceleration structure that holds all the instances of the scene.
	// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
	// the instances, computing the memory requirements for the AS, and building the AS itself.

	// Gather all the instances into the builder helper
	for (size_t i = 0; i < instances.size(); i++)
	{
		m_topLevelASGenerator.AddInstance(instances[i].first.Get(), instances[i].second, static_cast<UINT>(i), static_cast<UINT>(2 * i) /*because there are 2 hit groups per instance (shadow and normal)*/);
	}

	// As for the bottom-level AS, the building the AS requires some scratch space
	// to store temprary data in addition to the actual AS. In the case of the
	// top-level AS, the instance descriptors also need to be stored in GPU memory.
	// This call outputs the memory requirements for each (scratch, results, instance descriptors)
	// so that the application can allocate the corresponding memory.
	UINT64 scratchSize, resultSize, instanceDescsSize;
	m_topLevelASGenerator.ComputeASBufferSizes(m_device.Get(), true, &scratchSize, &resultSize, &instanceDescsSize);

	// Create the scratch and result buffers. Since the build is all done on GPU
	// those can be allocated on the default heap.
	m_topLevelASBuffers.pStratch = nv_helpers_dx12::CreateBuffer(m_device.Get(), scratchSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);

	m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(m_device.Get(), resultSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nv_helpers_dx12::kDefaultHeapProps);

	// The buffer describing the instances: ID, shader binding information,
	// matrices... Those will be copied into the buffer by the helper through
	// mapping, so the buffer has to be allocated on the upload heap.
	m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(m_device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	m_topLevelASGenerator.Generate(m_commandList.Get(), m_topLevelASBuffers.pStratch.Get(), 
		m_topLevelASBuffers.pResult.Get(), m_topLevelASBuffers.pInstanceDesc.Get());
}

void D3D12HelloTriangle::CreateRaytracingPipeline()
{
	// The raytracing pipeline binds the shader code, root signatures and pipeline
	// characteristics in a single structure used by DXR to invoke the shaders and
	// manage temporary memory during raytracing.

	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device.Get());

	// The pipeline contains the DXIL code of all the shaders potentially executed
	// during the raytracing process. This section compiles the HLSL code into a set
	// of DXIL libraries. We chose to seperate the code in several libraries by
	// semantic (ray generation, hit, miss) for clarity. Any code layout can be used.
	m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"shaders/RayGen.hlsl");
	m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"shaders/Miss.hlsl");
	m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"shaders/Hit.hlsl");
	m_shadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"shaders/ShadowRay.hlsl");

	pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
	pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
	pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit", L"PlaneClosestHit" });
	pipeline.AddLibrary(m_shadowLibrary.Get(), { L"ShadowClosestHit", L"ShadowMiss" });

	// To be used, each DX12 shader needs a root signature defining which
	// parameters and buffers will be accessed.
	m_rayGenSignature = CreateRayGenSignature();
	m_missSignature = CreateMissSignature();
	m_hitSignature = CreateHitSignature();
	m_shadowSignature = CreateHitSignature();

	// 3 different shaders can be invoked to obtain an intersection: an
	// intersection shader is called
	// when hitting the bounding box of non-triangular geometry. This is beyond
	// the scope of this tutorial. An any-hit shader is called on potential
	// intersections. This shader can, for example, perform alpha-testing and
	// discard some intersections. Finally, the closest-hit program is invoked on
	// the intersection point closest to the ray origin. Those 3 shaders are bound
	// together into a hit group.

	// Note that for triangular geometry the intersection shader is built-in. An
	// empty any-hit shader is also defined by default, so in our simple case each
	// hit group contains only the closest hit shader. Note that since the
	// exported symbols are defined above the shaders can be simply referred to by
	// name.

	// Hit group for the triangles, with a shader simply interpolating vertex
	// colors
	pipeline.AddHitGroup(L"HitGroup", L"ClosestHit", L"", L"");
	pipeline.AddHitGroup(L"PlaneHitGroup", L"PlaneClosestHit");
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");

	// The following section associates the root signature to each shader. Note
	// that we can explicitly show that some shaders share the same root signature
	// (eg. Miss and ShadowMiss). Not that the hit shaders are now only referred
	// to as hit groups, meaning that the underlying intersection, any-hit and
	// closest-hit shaders share the same root signature.
	pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss", L"ShadowMiss" });
	pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup", L"PlaneHitGroup" });
	pipeline.AddRootSignatureAssociation(m_shadowSignature.Get(), { L"ShadowHitGroup" });

	// The payload size defines the maximum size of the data carried by the rays,
	// ie. the data exchanged between shaders, such as the HitInfo structure in 
	// the HLSL code. It is important to keep this value as low as possible as a 
	// too high value would result in unnecessary memory consumption and cache trashing.
	pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance

	// Upon hitting a surface, DXR can provide several attributes to the hit. In
	// our sample we just use the barycentric coordinates defined by the weights
	// u,v of the last two vertices of the triangle. The actual barycentrics can
	// be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
	pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

	// The raytracing process can shoot rays from existing hit points, resulting
	// in nested TraceRay calls. Our sample code traces only primary rays, which
	// then requires a trace depth of 1. Note that this recursion depth should be
	// kept to a minimum for best performance. Path tracing algorithms can be
	// easily flattened into a simple loop in the ray generation.
	pipeline.SetMaxRecursionDepth(2);

	// Compile the pipeline for execution on the GPU
	m_rtStateObject = pipeline.Generate();

	// Cas the state object into a properties object, allowing to later access
	// the shader pointers by name.
	ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateRayGenSignature() const
{
	// The ray generation shader needs to access 2 resources: The raytracing output
	// and the top-level acceleration structure.

	nv_helpers_dx12::RootSignatureGenerator rsg;
	rsg.AddHeapRangesParameter(
	{ 
		{0 /*u0*/, 1 /*1 descriptor*/, 0 /*use the implicit register space 0*/, 
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV /*UAV representing the output buffer*/, 0 /*heap slot where the UAV is defined*/},

		{0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 1},

		{0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 2}
	});

	return rsg.Generate(m_device.Get(), true);
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateMissSignature() const
{
	// The miss shader communicates only through the ray payload, and therefore
	// does not require any resources.

	nv_helpers_dx12::RootSignatureGenerator rsg;
	return rsg.Generate(m_device.Get(), true);
}

ComPtr<ID3D12RootSignature> D3D12HelloTriangle::CreateHitSignature() const
{
	nv_helpers_dx12::RootSignatureGenerator rsg;
	rsg.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);

	// DXR extra: Per-instance data
	// The vertex colors may differ for each instance, so it is not possible to
	// point to a single buffer in the heap. Instead we use the concept of root
	// parameters, which are defined directly by a pointer in memory. In the
	// shader binding table we will associate each hit shader instance with its
	// constant buffer. Here we bind the buffer to the first slot, accessible in
	// HLSL as register(b0).
	rsg.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);

	// DXR extra: Another ray type
	// Add a single range pointing to the TLAS in the heap
	rsg.AddHeapRangesParameter({ {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 /*2nd slot of the heap*/} });

	return rsg.Generate(m_device.Get(), true);
}

void D3D12HelloTriangle::CreateRaytracingOutputBuffer()
{
	// Allocate the buffer holding the raytracing output, with the same size as
	// the output image.

	D3D12_RESOURCE_DESC resDesc{};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; 
	
	// The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNFORM_SRGB, but sRGB
	// formats cannot be used with UAVs. For accuracy we should convert to sRGB
	// ourselved in the shader.
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = GetWidth();
	resDesc.Height = GetHeight();
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;

	ThrowIfFailed(m_device->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputResource)));
}

void D3D12HelloTriangle::CreateShaderResourceHeap()
{
	// Create the main heap used by the shaders, which will give access to the
	// raytracing output and the top-level acceleration structure.

	// Create a SRV/UAV/CBV descriptor heap. We need 3 entries: 1 UAV for the
	// raytracing output and 1 SRV for the TLAS and 1 CBV for the camera matrices.
	m_srcUavHeap = nv_helpers_dx12::CreateDescriptorHeap(m_device.Get(), 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Get a handle to the heap memory on the CPU side, so be able to write the
	// descriptors directly.
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srcUavHeap->GetCPUDescriptorHandleForHeapStart();

	// Create the UAV. Based on the root signature we created it is the first
	// entry. The Create*View methods write the view information directly info srvHandle.
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	m_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, srvHandle);

	// Add the Top-level AS SRV right after the raytracing output buffer.
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress();

	// Write the acceleration structure view in the heap.
	m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

	// Add the constant buffer for the camera
	srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_cameraBufferSize;
	
	m_device->CreateConstantBufferView(&cbvDesc, srvHandle);
}

void D3D12HelloTriangle::CreateShaderBindingTable()
{
	// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
	// this is where the shader resources are bound to the shaders, in a way that
	// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
	// contains a series of shader IDs with their resource pointers. The SBT
	// contains the ray generation shader, the miss shaders, then the hit groups.
	// Using the helper class, those can be specified in arbitrary order.

	// The SBT helper class collects callt to Add*Program. If called several
	// times, the helper must be emptied before re-adding shaders.
	m_sbtHelper.Reset();

	// The pointer to the beginning of the heap is the only parameter required by
	// shaders without root parameters.
	D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srcUavHeap->GetGPUDescriptorHandleForHeapStart();

	// The helper treats both root parameter pointers and heap pointers as void*,
	// while DX12 uses the D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers.
	// The pointer in this struct is a UINT64, which then has to be reinterpreted
	// as a pointer.
	UINT64* heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);

	// The ray generation only uses heap data
	m_sbtHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });

	// The miss and hit shaders do not access any external resources: instead they
	// communicate their results through the ray payload.
	m_sbtHelper.AddMissProgram(L"Miss", {});
	m_sbtHelper.AddMissProgram(L"ShadowMiss", {});

	//// Adding the triangle hit shader
	//m_sbtHelper.AddHitGroup(L"HitGroup", { 
	//	(void*) m_vertexBuffer->GetGPUVirtualAddress(), 
	//	(void*)m_globalConstantBuffer->GetGPUVirtualAddress() 
	//});

	// We have 3 triangles, each of which needs to access its own constant buffer
	// as a root parameter in its primary hit shader. The shadow hit only set a
	// boolean visibility in the payload, and does not require external data
	// Which hitgroup is linked to which geometry is done when assigning the hitgroupindex
	// when creating the top-level AS.
	for (int i = 0; i < 3; ++i)
	{
		m_sbtHelper.AddHitGroup(L"HitGroup", { (void*)m_vertexBuffer->GetGPUVirtualAddress(), (void*)m_perInstanceConstantBuffers[i]->GetGPUVirtualAddress() });
		m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});
	}
	// The plane also uses a constant buffer for its vertex colors
	m_sbtHelper.AddHitGroup(L"PlaneHitGroup", { (void*)m_vertexBuffer->GetGPUVirtualAddress(), (void*)m_perInstanceConstantBuffers[0]->GetGPUVirtualAddress(), heapPointer });
	m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});

	// Compute the size of the SBT given the number of shaders and their parameters.
	const uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();

	// Create the SBT on the upload heap. This is required as the helper will use
	// mapping to write the SBT contents. After the SBT compilation it could be
	// copied to the default heap for performance.
	m_sbtStorage = nv_helpers_dx12::CreateBuffer(m_device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	if (m_sbtStorage == nullptr)
	{
		throw std::logic_error("Could not allocate the shader binding table");
	}

	// Compile the SBT from the shader and parameters info.
	m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
}

void D3D12HelloTriangle::CreateCameraBuffer()
{
	// The camera buffer is a constant buffer that stores the transform matrices of
	// the camera, for use by both the raterization and raytracing. This method
	// allocates the buffer where the matrices will be copied. For the sake of code
	// clarity, it also creates a heap containing only this buffer, to use in the
	// rasterization path.

	// view, perspective, viewInc, perspectiveInv
	uint32_t nbMatrix = 4;
	m_cameraBufferSize = nbMatrix * sizeof(DirectX::XMMATRIX);

	// Create the constance buffer for all matrices
	m_cameraBuffer = nv_helpers_dx12::CreateBuffer(m_device.Get(), m_cameraBufferSize, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// Create a descriptor heap that will be used by the rasterization shaders
	m_constHeap = nv_helpers_dx12::CreateDescriptorHeap(m_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Describe and create the constant buffer view
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_cameraBufferSize;

	// Get a handle to the heap memory on the CPU side, to be able to wirte the
	// descriptors directly
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_constHeap->GetCPUDescriptorHandleForHeapStart();
	m_device->CreateConstantBufferView(&cbvDesc, srvHandle);
}

void D3D12HelloTriangle::UpdateCameraBuffer()
{
	// Create and copies the viewmodel and perspective matrices of the camera

	std::vector<DirectX::XMMATRIX> matrices(4);

	// Initialize the view matrix, ideally this should be based on used
	// interactions. The lookat and perspective matrices used for rasterization
	// are defined to transform world-space vertices into a [0,1]x[0,1]x[0,1]
	// camera space.
	//DirectX::XMVECTOR eye = DirectX::XMVectorSet(1.5f, 1.5f, 1.5f, 0.0f);
	DirectX::XMVECTOR eye = DirectX::XMVectorSet(m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z, 0.0f);
	DirectX::XMVECTOR at = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	//viewMatrix = viewMatrix * DirectX::XMMatrixRotationX(0);
	//viewMatrix = viewMatrix * DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(m_cameraRotation.y));
	//viewMatrix = viewMatrix * DirectX::XMMatrixRotationZ(0);
	//viewMatrix = viewMatrix * DirectX::XMMatrixTranslation(-m_cameraPosition.x, -m_cameraPosition.y, -m_cameraPosition.z);

	matrices[0] = DirectX::XMMatrixLookAtRH(eye, at, up);
	//matrices[0] = viewMatrix;


	float fovAngleY = 45.0f * DirectX::XM_PI / 180.0f;
	matrices[1] = DirectX::XMMatrixPerspectiveFovRH(fovAngleY, m_aspectRatio, 0.1f, 1000.0f);

	// Raytracing has to do the contrary of rasterization: rays are defined in
	// camera space, and are transformed into world space. To do this, we need to
	// store the inverse matrices as well.
	DirectX::XMVECTOR det;
	matrices[2] = DirectX::XMMatrixInverse(&det, matrices[0]);
	matrices[3] = DirectX::XMMatrixInverse(&det, matrices[1]);

	// Copy the matrix contents into the buffer
	uint8_t* pData;
	ThrowIfFailed(m_cameraBuffer->Map(0, nullptr, (void**)&pData));
	memcpy(pData, matrices.data(), m_cameraBufferSize);
	m_cameraBuffer->Unmap(0, nullptr);
}

void D3D12HelloTriangle::CreateDepthBuffer()
{
	// Create the depth buffer for rasterization. This buffer needs to be kept
	// in a seperate heap.

	// The depth buffer heap type is specific for that usage, and the heap contents 
	// are not visible from the shaders.
	m_dsvHeap = nv_helpers_dx12::CreateDescriptorHeap(m_device.Get(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

	// The depth and stencil can be packing into a single 32-bit texture buffer.
	// Since we do not need stencil, we use the 32 bits to store the depth information.
	D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 1);
	depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// The depth values will be initialized to 1
	CD3DX12_CLEAR_VALUE depthOptimizedClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	// Allocate the buffer itself, with a state allowing depth writes
	ThrowIfFailed(m_device->CreateCommittedResource(&depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthResourceDesc, 
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&m_depthStencil)));

	// Write the depth buffer view into the depth buffer heap
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12HelloTriangle::CreatePlaneVB()
{
	// Define the geometry for a plane
	Vertex planeVertices[] = 
	{ 
		{{-1.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 0 
		{{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1 
		{{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2 
		{{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2 
		{{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1 
		{{01.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}} // 4 
	};

	const UINT planeBufferSize = sizeof(planeVertices);

	// Note: using upload heap to transfer static data like vert buffers is
	// not recommended. Every time the GPU needs it, the upload heap will be
	// marshalled over. Please use Default Heap usage.
	CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(planeBufferSize);
	ThrowIfFailed(m_device->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_planeBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	// We do not intend to read from this resource on the CPU
	ThrowIfFailed(m_planeBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, planeVertices, planeBufferSize);
	m_planeBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view (only needed for rasterization)
	m_planeBufferView.BufferLocation = m_planeBuffer->GetGPUVirtualAddress();
	m_planeBufferView.StrideInBytes = sizeof(Vertex);
	m_planeBufferView.SizeInBytes = planeBufferSize;
}

void D3D12HelloTriangle::CreateGlobalConstantBuffer()
{
	// Due to HLSL packing rules, we create the CB with 9 float4 (each needs to start on a 16-byte boundary)
	DirectX::XMVECTOR bufferData[] = 
	{
		// A
		DirectX::XMVECTOR{1.0f, 0.0f, 0.0f, 1.0f},
		DirectX::XMVECTOR{0.7f, 0.4f, 0.0f, 1.0f},
		DirectX::XMVECTOR{0.4f, 0.7f, 0.0f, 1.0f},

		// B
		DirectX::XMVECTOR{0.0f, 1.0f, 0.0f, 1.0f},
		DirectX::XMVECTOR{0.0f, 0.7f, 0.4f, 1.0f},
		DirectX::XMVECTOR{0.0f, 0.4f, 0.7f, 1.0f},

		// C
		DirectX::XMVECTOR{0.0f, 0.0f, 1.0f, 1.0f},
		DirectX::XMVECTOR{0.4f, 0.0f, 0.7f, 1.0f},
		DirectX::XMVECTOR{0.7f, 0.0f, 0.4f, 1.0f}
	};

	// Create out buffer
	m_globalConstantBuffer = nv_helpers_dx12::CreateBuffer(m_device.Get(), sizeof(bufferData), D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// Copy the CPU memory to GPU
	uint8_t* pData;
	ThrowIfFailed(m_globalConstantBuffer->Map(0, nullptr, (void**) &pData));
	memcpy(pData, bufferData, sizeof(bufferData));
	m_globalConstantBuffer->Unmap(0, nullptr);
}

void D3D12HelloTriangle::CreatePerInstanceConstantBuffers()
{
	// Due to HLSL packing rules, we create the CB with 9 float4 (each needs to start on a 16-byte boundary)
	DirectX::XMVECTOR bufferData[] = 
	{ 
		// A 
		DirectX::XMVECTOR{1.0f, 0.0f, 0.0f, 1.0f}, 
		DirectX::XMVECTOR{1.0f, 0.4f, 0.0f, 1.0f}, 
		DirectX::XMVECTOR{1.f, 0.7f, 0.0f, 1.0f}, 
		
		// B 
		DirectX::XMVECTOR{0.0f, 1.0f, 0.0f, 1.0f}, 
		DirectX::XMVECTOR{0.0f, 1.0f, 0.4f, 1.0f}, 
		DirectX::XMVECTOR{0.0f, 1.0f, 0.7f, 1.0f}, 
		
		// C 
		DirectX::XMVECTOR{0.0f, 0.0f, 1.0f, 1.0f}, 
		DirectX::XMVECTOR{0.4f, 0.0f, 1.0f, 1.0f}, 
		DirectX::XMVECTOR{0.7f, 0.0f, 1.0f, 1.0f}
	};

	m_perInstanceConstantBuffers.resize(3);
	int i = 0;
	for (auto& cb : m_perInstanceConstantBuffers)
	{
		const uint32_t bufferSize = sizeof(DirectX::XMVECTOR) * 3;
		cb = nv_helpers_dx12::CreateBuffer(m_device.Get(), bufferSize, D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

		uint8_t* pData;
		ThrowIfFailed(cb->Map(0, nullptr, (void**) &pData));
		memcpy(pData, &bufferData[i * 3], bufferSize);
		cb->Unmap(0, nullptr);
		++i;
	}
}
