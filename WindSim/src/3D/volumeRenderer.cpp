#include "volumeRenderer.h"
#include "common.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"
#include <comdef.h>

#include <cmath>

using namespace DirectX;

ID3DX11Effect* VolumeRenderer::s_effect = nullptr;
VolumeRenderer::ShaderVariables VolumeRenderer::s_shaderVariables;

HRESULT VolumeRenderer::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
{
	HRESULT hr;

	releaseShader();

	if (reload)
	{
		ID3DBlob* blob;
		if (D3DX11CompileEffectFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, D3DCOMPILE_DEBUG, 0, device, &s_effect, &blob) != S_OK)
		{
			char* buffer = reinterpret_cast<char*>(blob->GetBufferPointer());
			OutputDebugStringA(buffer);
			return E_FAIL;
		}
	}
	else
	{
		hr = (D3DX11CreateEffectFromFile(shaderPath.c_str(), 0, device, &s_effect));
		if (FAILED(hr))
		{
			_com_error err(hr);
			OutputDebugString((L"ERROR: Failed to create effect from file with message '" + std::wstring(err.ErrorMessage()) + L"'\n").c_str());
			return S_FALSE;
		}
	}

	s_shaderVariables.scalarGridUAV    = s_effect->GetVariableByName("g_scalarGridUAV")->AsUnorderedAccessView();
	s_shaderVariables.scalarGridSRV    = s_effect->GetVariableByName("g_scalarGridSRV")->AsShaderResource();
	s_shaderVariables.vectorGrid       = s_effect->GetVariableByName("g_vectorGrid")->AsShaderResource();
	s_shaderVariables.depthTex         = s_effect->GetVariableByName("g_depthTex")->AsShaderResource();

	s_shaderVariables.proj             = s_effect->GetVariableByName("g_mProj")->AsMatrix();
	s_shaderVariables.projInv          = s_effect->GetVariableByName("g_mProjInv")->AsMatrix();
	s_shaderVariables.worldView        = s_effect->GetVariableByName("g_mWorldView")->AsMatrix();
	s_shaderVariables.worldViewProjInv = s_effect->GetVariableByName("g_mWorldViewProjInv")->AsMatrix();
	s_shaderVariables.texToGrid        = s_effect->GetVariableByName("g_mTexToGrid")->AsMatrix();
	s_shaderVariables.texToGridInv     = s_effect->GetVariableByName("g_mTexToGridInv")->AsMatrix();

	s_shaderVariables.dimensions       = s_effect->GetVariableByName("g_vDimensions")->AsVector();
	s_shaderVariables.camPosTS         = s_effect->GetVariableByName("g_vCamPosTS")->AsVector();

	return S_OK;
}

void VolumeRenderer::releaseShader()
{
	SAFE_RELEASE(s_effect);
}

VolumeRenderer::VolumeRenderer()
	: m_scalarGrid(nullptr)
	, m_scalarGridUAV(nullptr)
	, m_scalarGridSRV(nullptr)
{
}

HRESULT VolumeRenderer::create(ID3D11Device* device, const DirectX::XMUINT3 gridDimensions)
{
	HRESULT hr;

	// Texture 3D, holding 32bit scalars
	D3D11_TEXTURE3D_DESC td;
	td.Width = gridDimensions.x;
	td.Height = gridDimensions.y;
	td.Depth = gridDimensions.z;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R32_FLOAT;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0;
	td.MiscFlags = 0;

	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_scalarGrid));

	V_RETURN(device->CreateUnorderedAccessView(m_scalarGrid, nullptr, &m_scalarGridUAV));

	V_RETURN(device->CreateShaderResourceView(m_scalarGrid, nullptr, &m_scalarGridSRV));

	return S_OK;
}

void VolumeRenderer::release()
{
	SAFE_RELEASE(m_scalarGridUAV);
	SAFE_RELEASE(m_scalarGridSRV);
	SAFE_RELEASE(m_scalarGrid);
}

void VolumeRenderer::render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* vectorGrid, ID3D11ShaderResourceView* depthStencilView, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, const XMUINT3 gridDimensions, const XMFLOAT3& voxelSize)
{
	s_shaderVariables.dimensions->SetIntVector(reinterpret_cast<const int*>(&gridDimensions));

	// Generate scalar grid from vector grid
	s_shaderVariables.vectorGrid->SetResource(vectorGrid);
	s_shaderVariables.scalarGridUAV->SetUnorderedAccessView(m_scalarGridUAV);

	s_effect->GetTechniqueByName("Metric")->GetPassByName("Vorticity")->Apply(0, context);

	XMUINT3 dispatch =
	{
		static_cast<uint32_t>(std::ceil(gridDimensions.x / 16)),
		static_cast<uint32_t>(std::ceil(gridDimensions.y / 8)),
		static_cast<uint32_t>(std::ceil(gridDimensions.z / 8))
	};
	context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

	s_shaderVariables.vectorGrid->SetResource(nullptr);
	s_shaderVariables.scalarGridUAV->SetUnorderedAccessView(nullptr);
	s_effect->GetTechniqueByName("Metric")->GetPassByName("Vorticity")->Apply(0, context);


	// Direct volume rendering of generated scalar grid
	XMMATRIX mWorld = XMLoadFloat4x4(&world);
	XMMATRIX mView = XMLoadFloat4x4(&view);
	XMMATRIX mProj = XMLoadFloat4x4(&projection);

	XMMATRIX mProjInv = XMMatrixInverse(nullptr, mProj);
	XMMATRIX mWorldView = mWorld * mView;
	XMMATRIX mWorldViewProjInv = XMMatrixInverse(nullptr, mWorld * mView * mProj);
	XMVECTOR volumeSize = XMVectorMultiply(XMLoadUInt3(&gridDimensions), XMLoadFloat3(&voxelSize));
	XMMATRIX mTexToGrid = XMMatrixScalingFromVector(volumeSize);
	XMMATRIX mTexToGridInv = XMMatrixScalingFromVector(XMVectorReciprocal(volumeSize));

	// Calculate camera position in texture space ([0, 1]^3)
	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, mView)); // World Space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, mWorld)); // Grid Object Space
	camPos = XMVector3Transform(camPos, mTexToGridInv); // Texture Space

	s_shaderVariables.scalarGridSRV->SetResource(m_scalarGridSRV);
	s_shaderVariables.depthTex->SetResource(depthStencilView);

	s_shaderVariables.proj->SetMatrix(mProj.r->m128_f32);
	s_shaderVariables.projInv->SetMatrix(mProjInv.r->m128_f32);
	s_shaderVariables.worldView->SetMatrix(mWorldView.r->m128_f32);
	s_shaderVariables.worldViewProjInv->SetMatrix(mWorldViewProjInv.r->m128_f32);
	s_shaderVariables.texToGrid->SetMatrix(mTexToGrid.r->m128_f32);
	s_shaderVariables.texToGridInv->SetMatrix(mTexToGridInv.r->m128_f32);

	s_shaderVariables.camPosTS->SetRawValue(camPos.m128_f32, 0, sizeof(float) * 3);

	s_effect->GetTechniqueByName("Render")->GetPassByName("Direct")->Apply(0, context);

	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = 0;
	UINT offset = 0;
	context->IASetVertexBuffers(0, 0, nullptr, &stride, &offset);
	context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	context->Draw(3, 0);

	s_shaderVariables.scalarGridSRV->SetResource(nullptr);
	s_shaderVariables.depthTex->SetResource(nullptr);

	s_effect->GetTechniqueByName("Render")->GetPassByName("Direct")->Apply(0, context);
}
