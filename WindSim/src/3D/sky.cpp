#include "sky.h"
#include "common.h"

#include "d3dx11effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>

using namespace DirectX;

Sky::Sky(Logger* logger)
	: Object3D(logger)
{
	createCubeData();
	m_numIndices = m_indexData.size();
}

Sky::Sky(Sky&& other)
	: Object3D(std::move(other))
{
}

Sky::~Sky()
{
}

HRESULT Sky::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
{
	HRESULT hr;

	releaseShader();

	if (reload)
	{
		ID3DBlob* blob;
		if (D3DX11CompileEffectFromFile(shaderPath.c_str(), nullptr, nullptr, D3DCOMPILE_DEBUG, 0, device, &s_effect, &blob) != S_OK)
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

	s_worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();

	// One float3, used as postion and cubetexture coordinate
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_inputLayout));

	return S_OK;
}

void Sky::releaseShader()
{
	SAFE_RELEASE(s_inputLayout);
	SAFE_RELEASE(s_effect);
}

void Sky::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (s_effect == nullptr)
	{
		return;
	}

	XMMATRIX worldViewProj = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);

	s_worldViewProj->SetMatrix(reinterpret_cast<float*>(worldViewProj.r));

	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void Sky::createCubeData()
{
	// Cube [-1, 1]^3
	m_vertexData =
	{
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f
	};

	m_indexData =
	{
		0, 1, 3,
		4, 7, 6,
		0, 4, 5,
		1, 5, 2,
		2, 6, 3,
		4, 0, 3,
		1, 2, 3,
		5, 4, 6,
		1, 0, 5,
		5, 6, 2,
		6, 7, 3,
		7, 4, 3
	};
}


ID3DX11EffectMatrixVariable* Sky::s_worldViewProj = nullptr;
ID3DX11Effect* Sky::s_effect = nullptr;
ID3D11InputLayout* Sky::s_inputLayout = nullptr;