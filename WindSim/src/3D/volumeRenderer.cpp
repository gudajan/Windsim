#include "volumeRenderer.h"
#include "common.h"

#include <QPropertyAnimation>

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
	s_shaderVariables.txfnTex          = s_effect->GetVariableByName("g_txfnTex")->AsShaderResource();

	s_shaderVariables.proj             = s_effect->GetVariableByName("g_mProj")->AsMatrix();
	s_shaderVariables.projInv          = s_effect->GetVariableByName("g_mProjInv")->AsMatrix();
	s_shaderVariables.worldView        = s_effect->GetVariableByName("g_mWorldView")->AsMatrix();
	s_shaderVariables.worldViewProjInv = s_effect->GetVariableByName("g_mWorldViewProjInv")->AsMatrix();
	s_shaderVariables.texToGrid        = s_effect->GetVariableByName("g_mTexToGrid")->AsMatrix();
	s_shaderVariables.texToGridInv     = s_effect->GetVariableByName("g_mTexToGridInv")->AsMatrix();

	s_shaderVariables.dimensions       = s_effect->GetVariableByName("g_vDimensions")->AsVector();
	s_shaderVariables.voxelSize        = s_effect->GetVariableByName("g_vVoxelSize")->AsVector();
	s_shaderVariables.camPosOS         = s_effect->GetVariableByName("g_vCamPosOS")->AsVector();

	s_shaderVariables.rangeMin         = s_effect->GetVariableByName("g_sRangeMin")->AsScalar();
	s_shaderVariables.rangeMax         = s_effect->GetVariableByName("g_sRangeMax")->AsScalar();
	s_shaderVariables.stepSize         = s_effect->GetVariableByName("g_sStepSize")->AsScalar();

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
	, m_txfnTex(nullptr)
	, m_txfnSRV(nullptr)
	, m_enabled(false)
	, m_metric(Metric::Magnitude)
	, m_rangeMin(0.0)
	, m_rangeMax(1000.0)
	, m_stepSize(0.5)
{
}

HRESULT VolumeRenderer::create(ID3D11Device* device, const DirectX::XMUINT3 gridDimensions)
{
	release();

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

	return createTxFn(device, TransferFunction());
}

void VolumeRenderer::release()
{
	SAFE_RELEASE(m_scalarGridUAV);
	SAFE_RELEASE(m_scalarGridSRV);
	SAFE_RELEASE(m_scalarGrid);
	SAFE_RELEASE(m_txfnTex);
	SAFE_RELEASE(m_txfnSRV);
}

void VolumeRenderer::render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* vectorGrid, ID3D11ShaderResourceView* depthStencilView, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, const XMUINT3 gridDimensions, const XMFLOAT3& voxelSize)
{
	if (!m_enabled) return;

	s_shaderVariables.dimensions->SetIntVector(reinterpret_cast<const int32_t*>(&gridDimensions));

	// Generate scalar grid from vector grid
	s_shaderVariables.vectorGrid->SetResource(vectorGrid);
	s_shaderVariables.scalarGridUAV->SetUnorderedAccessView(m_scalarGridUAV);

	std::string metric = Metric::toString(m_metric).toStdString();

	s_effect->GetTechniqueByName("Metric")->GetPassByName(metric.c_str())->Apply(0, context);

	XMUINT3 dispatch =
	{
		static_cast<uint32_t>(std::ceil(gridDimensions.x / 16)),
		static_cast<uint32_t>(std::ceil(gridDimensions.y / 8)),
		static_cast<uint32_t>(std::ceil(gridDimensions.z / 8))
	};
	context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

	s_shaderVariables.vectorGrid->SetResource(nullptr);
	s_shaderVariables.scalarGridUAV->SetUnorderedAccessView(nullptr);
	s_effect->GetTechniqueByName("Metric")->GetPassByName(metric.c_str())->Apply(0, context);


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

	// Calculate camera position in grid object space
	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, mView)); // World Space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, mWorld)); // Grid Object Space

	s_shaderVariables.scalarGridSRV->SetResource(m_scalarGridSRV);
	s_shaderVariables.depthTex->SetResource(depthStencilView);
	s_shaderVariables.txfnTex->SetResource(m_txfnSRV);

	s_shaderVariables.proj->SetMatrix(mProj.r->m128_f32);
	s_shaderVariables.projInv->SetMatrix(mProjInv.r->m128_f32);
	s_shaderVariables.worldView->SetMatrix(mWorldView.r->m128_f32);
	s_shaderVariables.worldViewProjInv->SetMatrix(mWorldViewProjInv.r->m128_f32);
	s_shaderVariables.texToGrid->SetMatrix(mTexToGrid.r->m128_f32);
	s_shaderVariables.texToGridInv->SetMatrix(mTexToGridInv.r->m128_f32);

	s_shaderVariables.voxelSize->SetFloatVector(&voxelSize.x);
	s_shaderVariables.camPosOS->SetRawValue(camPos.m128_f32, 0, sizeof(float) * 3);

	s_shaderVariables.rangeMin->SetFloat(static_cast<float>(m_rangeMin));
	s_shaderVariables.rangeMax->SetFloat(static_cast<float>(m_rangeMax));
	s_shaderVariables.stepSize->SetFloat(static_cast<float>(m_stepSize));

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
	s_shaderVariables.txfnTex->SetResource(nullptr);

	s_effect->GetTechniqueByName("Render")->GetPassByName("Direct")->Apply(0, context);
}

void VolumeRenderer::changeSettings(ID3D11Device* device, const QJsonObject& settings, bool createFunction)
{
	m_enabled = settings["enabled"].toBool();
	const QString& metric = settings["metric"].toString();
	m_metric = Metric::toMetric(metric);

	TransferFunction txfn = TransferFunction::fromJson(settings["transferFunctions"].toObject()[metric].toObject());

	if (createFunction)
		createTxFn(device, txfn);

	m_rangeMin = txfn.rangeMin;
	m_rangeMax = txfn.rangeMax;

	m_stepSize = max(0.001, settings["stepSize"].toDouble());
}

HRESULT VolumeRenderer::createTxFn(ID3D11Device* device, const TransferFunction& txfn)
{
	SAFE_RELEASE(m_txfnTex);
	SAFE_RELEASE(m_txfnSRV);

	int resolution = 64;

	HRESULT hr;

	D3D11_TEXTURE1D_DESC td;
	td.Width = resolution;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0;
	td.MiscFlags = 0;

	std::vector<char> tex;
	txfn.getTexR8G8B8A8(tex, resolution);

	D3D11_SUBRESOURCE_DATA sd;
	sd.pSysMem = tex.data();

	V_RETURN(device->CreateTexture1D(&td, &sd, &m_txfnTex));

	V_RETURN(device->CreateShaderResourceView(m_txfnTex, nullptr, &m_txfnSRV));

	return S_OK;
}