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

#pragma once

#include "DXSample.h"
#include <vector>
#include <dxcapi.h>
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTriangle final : public DXSample
{
public:
	D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

	void OnInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnDestroy() override;
	void OnResize() override;

	void OnKeyUp(UINT8 key) override;
	void OnKeyDown(UINT8 key) override;
	void OnMouseMove(UINT8 button, UINT32 movement) override;

private:
	static const UINT FrameCount = 2;

	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device5> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList4> m_commandList;
	UINT m_rtvDescriptorSize;

	// App resources.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	bool m_raster = false;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList() const;
	void WaitForPreviousFrame();
	void CheckRaytracingSupport();

	// DXR

	struct AccelerationStructureBuffers
	{
		ComPtr<ID3D12Resource> pStratch;		// Scratch memory for AS builder
		ComPtr<ID3D12Resource> pResult;			// Where the AS actual is
		ComPtr<ID3D12Resource> pInstanceDesc;	// Hold the matrices of the instances
	};

	ComPtr<ID3D12Resource> m_bottomLevelAS;		// Storage for the bottom level AS
	nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
	AccelerationStructureBuffers m_topLevelASBuffers;
	std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> m_instances;

	/*
	* Create all acceleration structures, bottom and top
	*/
	void CreateAccelerationStructures();

	/*
	* Create the acceleration structure of an instance.
	* 
	* @param vVertexBuffers : Pair of vertex buffer and vertex count
	* @return AccelerationStructureBuffers for TLAS
	*/
	AccelerationStructureBuffers CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers);

	/*
	* Create the main acceleration structure that holds all instances of the scene.
	* 
	*  @param instances Pair of BLAS and transform
	*/
	void CreateTopLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances);

	void CreateRaytracingPipeline();

	ComPtr<ID3D12RootSignature> CreateRayGenSignature() const;
	ComPtr<ID3D12RootSignature> CreateMissSignature() const;
	ComPtr<ID3D12RootSignature> CreateHitSignature() const;

	ComPtr<IDxcBlob> m_rayGenLibrary;
	ComPtr<IDxcBlob> m_hitLibrary;
	ComPtr<IDxcBlob> m_missLibrary;

	ComPtr<ID3D12RootSignature> m_rayGenSignature;
	ComPtr<ID3D12RootSignature> m_hitSignature;
	ComPtr<ID3D12RootSignature> m_missSignature;

	// Ray tracing pipeline state
	ComPtr<ID3D12StateObject> m_rtStateObject;
	// Ray tracing pipeline state properties, retaining the shader identifiers
	// to use in the Shader Binding Table
	ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

	void CreateRaytracingOutputBuffer();
	void CreateShaderResourceHeap();
	ComPtr<ID3D12Resource> m_outputResource;
	ComPtr<ID3D12DescriptorHeap> m_srcUavHeap;

	void CreateShaderBindingTable();
	nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
	ComPtr<ID3D12Resource> m_sbtStorage;

	// DXR extra: Perspective camera
	void CreateCameraBuffer();
	void UpdateCameraBuffer();
	ComPtr<ID3D12Resource> m_cameraBuffer;
	ComPtr<ID3D12DescriptorHeap> m_constHeap;
	uint32_t m_cameraBufferSize = 0;
	DirectX::XMFLOAT3 m_cameraPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 1.5f);
	DirectX::XMFLOAT3 m_cameraRotation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	const float CAMERA_SPEED = 0.1;

	// DXR extra: Depth buffering (only applies to rasterizer)
	void CreateDepthBuffer();
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthStencil;

	// DXR extra: Per-instance data
	ComPtr<ID3D12Resource> m_planeBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_planeBufferView;
	void CreatePlaneVB();
	void CreateGlobalConstantBuffer();
	ComPtr<ID3D12Resource> m_globalConstantBuffer;
	void CreatePerInstanceConstantBuffers();
	std::vector<ComPtr<ID3D12Resource>> m_perInstanceConstantBuffers;

	// DXR extra: Another ray type (shadows)
	ComPtr<IDxcBlob> m_shadowLibrary;
	ComPtr<ID3D12RootSignature> m_shadowSignature;
};
