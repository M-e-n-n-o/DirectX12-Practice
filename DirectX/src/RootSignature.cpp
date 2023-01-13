#include "dxpch.h"
#include "RootSignature.h"
#include "Application.h"

RootSignature::RootSignature()
    : m_rootSignatureDesc{}
    , m_numDescriptorsPerTable{ 0 }
    , m_samplerTableBitMask(0)
    , m_descriptorTableBitMask(0)
{}

RootSignature::RootSignature(
    const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
    : m_rootSignatureDesc{}
    , m_numDescriptorsPerTable{ 0 }
    , m_samplerTableBitMask(0)
    , m_descriptorTableBitMask(0)
{
    setRootSignatureDesc(rootSignatureDesc, rootSignatureVersion);
}

RootSignature::~RootSignature()
{
    destroy();
}

void RootSignature::destroy()
{
    for (UINT i = 0; i < m_rootSignatureDesc.NumParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1& rootParameter = m_rootSignatureDesc.pParameters[i];
        if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            delete[] rootParameter.DescriptorTable.pDescriptorRanges;
        }
    }

    delete[] m_rootSignatureDesc.pParameters;
    m_rootSignatureDesc.pParameters = nullptr;
    m_rootSignatureDesc.NumParameters = 0;

    delete[] m_rootSignatureDesc.pStaticSamplers;
    m_rootSignatureDesc.pStaticSamplers = nullptr;
    m_rootSignatureDesc.NumStaticSamplers = 0;

    m_descriptorTableBitMask = 0;
    m_samplerTableBitMask = 0;

    memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));
}

void RootSignature::setRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion)
{
    // Make sure any previously allocated root signature description is cleaned 
    // up first.
    destroy();

    auto device = Application::Get()->getDevice();

    UINT numParameters = rootSignatureDesc.NumParameters;
    D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

    for (UINT i = 0; i < numParameters; ++i)
    {
        const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
        pParameters[i] = rootParameter;

        if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
            D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

            memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges,
                sizeof(D3D12_DESCRIPTOR_RANGE1) * numDescriptorRanges);

            pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
            pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

            // Set the bit mask depending on the type of descriptor table.
            if (numDescriptorRanges > 0)
            {
                switch (pDescriptorRanges[0].RangeType)
                {
                case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                    m_descriptorTableBitMask |= (1 << i);
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    m_samplerTableBitMask |= (1 << i);
                    break;
                }
            }

            // Count the number of descriptors in the descriptor table.
            for (UINT j = 0; j < numDescriptorRanges; ++j)
            {
                m_numDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;
            }
        }
    }

    m_rootSignatureDesc.NumParameters = numParameters;
    m_rootSignatureDesc.pParameters = pParameters;

    UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
    D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

    if (pStaticSamplers)
    {
        memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers,
            sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
    }

    m_rootSignatureDesc.NumStaticSamplers = numStaticSamplers;
    m_rootSignatureDesc.pStaticSamplers = pStaticSamplers;

    D3D12_ROOT_SIGNATURE_FLAGS flags = rootSignatureDesc.Flags;
    m_rootSignatureDesc.Flags = flags;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
    versionRootSignatureDesc.Init_1_1(numParameters, pParameters, numStaticSamplers, pStaticSamplers, flags);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc,
        rootSignatureVersion, &rootSignatureBlob, &errorBlob));

    // Create the root signature.
    ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

uint32_t RootSignature::getDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const
{
    uint32_t descriptorTableBitMask = 0;
    switch (descriptorHeapType)
    {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
        descriptorTableBitMask = m_descriptorTableBitMask;
        break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
        descriptorTableBitMask = m_samplerTableBitMask;
        break;
    }

    return descriptorTableBitMask;
}

uint32_t RootSignature::getNumDescriptors(uint32_t rootIndex) const
{
    assert(rootIndex < 32);
    return m_numDescriptorsPerTable[rootIndex];
}