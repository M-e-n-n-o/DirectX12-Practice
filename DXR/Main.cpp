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
#include "Win32Application.h"

_Use_decl_annotations_
int WINAPI WinMain(const HINSTANCE hInstance, HINSTANCE, LPSTR, const int nShowCmd)
{
	D3D12HelloTriangle sample(1280, 720, L"D3D12 Hello Triangle");
	return Win32Application::Run(&sample, hInstance, nShowCmd);
}
