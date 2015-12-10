#include "marker.h"
#include "common.h"
#include "settings.h"

#include "d3dx11effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>

using namespace DirectX;

Marker::Marker()
	: Object3D(),
	m_renderPosition(true)
{
	m_vertexData.insert(m_vertexData.end(), { 0.0f, 0.0f, 0.0f }); // Push pack origin position
	m_indexData.push_back(0); // One point
	m_numIndices = m_indexData.size();
}

Marker::Marker(Marker&& other)
{
}

Marker::~Marker()
{
}

HRESULT Marker::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
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
		V_RETURN(D3DX11CreateEffectFromFile(shaderPath.c_str(), 0, device, &s_effect));
	}

	s_shaderVariables.worldView = s_effect->GetVariableByName("g_mWorldView")->AsMatrix();
	s_shaderVariables.projection = s_effect->GetVariableByName("g_mProjection")->AsMatrix();
	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
	s_shaderVariables.renderX = s_effect->GetVariableByName("g_bRenderX")->AsScalar();
	s_shaderVariables.renderY = s_effect->GetVariableByName("g_bRenderY")->AsScalar();
	s_shaderVariables.renderZ = s_effect->GetVariableByName("g_bRenderZ")->AsScalar();
	s_shaderVariables.renderLarge = s_effect->GetVariableByName("g_bRenderLarge")->AsScalar();

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_inputLayout));

	return S_OK;
}

void Marker::releaseShader()
{
	SAFE_RELEASE(s_inputLayout);
	SAFE_RELEASE(s_effect);
}


void Marker::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	if (s_effect == nullptr)
	{
		return;
	}
	//if (m_vertexBuffer == nullptr)
	//{
	//	Object3D::create(device, true);
	//}

	XMMATRIX proj = XMLoadFloat4x4(&projection);
	XMMATRIX worldView = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view);
	XMMATRIX worldViewProj = worldView * proj;

	s_shaderVariables.worldView->SetMatrix(reinterpret_cast<float*>(worldView.r));
	s_shaderVariables.projection->SetMatrix(reinterpret_cast<float*>(proj.r));
	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>(worldViewProj.r));

	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };

	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	if (m_renderPosition)
	{
		s_effect->GetTechniqueByIndex(0)->GetPassByName("Position")->Apply(0, context);
		context->DrawIndexed(m_numIndices, 0, 0);
	}

	s_effect->GetTechniqueByIndex(0)->GetPassByName("Axes")->Apply(0, context);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void Marker::setShaderVariables(bool renderX, bool renderY, bool renderZ, bool renderLarge)
{
	s_shaderVariables.renderX->SetBool(renderX);
	s_shaderVariables.renderY->SetBool(renderY);
	s_shaderVariables.renderZ->SetBool(renderZ);
	s_shaderVariables.renderLarge->SetBool(renderLarge);
}

Marker::ShaderVariables::ShaderVariables()
	: worldView(nullptr),
	projection(nullptr),
	worldViewProj(nullptr),
	renderX(nullptr),
	renderY(nullptr),
	renderZ(nullptr),
	renderLarge(nullptr)

{
}

Marker::ShaderVariables::~ShaderVariables()
{
}

ID3DX11Effect* Marker::s_effect = nullptr;
Marker::ShaderVariables Marker::s_shaderVariables;
ID3D11InputLayout* Marker::s_inputLayout = nullptr;