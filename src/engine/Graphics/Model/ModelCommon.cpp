#include "ModelCommon.h"
#include "../System/DirectXCommon.h"
#include "../System/SrvManager.h"
#include <cassert>

void ModelCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateSkinningPipeline();
}

void ModelCommon::CreateSkinningPipeline() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
	srvRange[0].BaseShaderRegister = 0;
	srvRange[0].NumDescriptors = 1;
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE uavRange[1] = {};
	uavRange[0].BaseShaderRegister = 0;
	uavRange[0].NumDescriptors = 1;
	uavRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// RootParam 0: CBV b0 (SkinningParams)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// RootParam 1: CBV b1 (BoneMatrix)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	// RootParam 2: SRV t0 (Input Vertices)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = srvRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// RootParam 3: UAV u0 (Output Vertices)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = uavRange;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(L"assets/shaders/Skinning.CS.hlsl", L"cs_6_0");
	assert(csBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = skinningRootSignature_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&skinningPipelineState_));
	assert(SUCCEEDED(hr));
}