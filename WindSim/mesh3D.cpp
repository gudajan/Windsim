#include "mesh3D.h"
#include "objLoader.h"
#include "common.h"

#include "d3dx11effect.h"
#include <d3dcompiler.h>
#include <d3d11.h>

using namespace DirectX;

Mesh3D::Mesh3D(const std::string& path)
	: Object3D()
{
	if (!readObj(path))
	{
		throw(std::runtime_error("Could not load obj-file '" + path + "' into 3D object!"));
	}
	m_numIndices = m_indexData.size();
}

Mesh3D::Mesh3D(Mesh3D&& other)
{
}

Mesh3D::~Mesh3D()
{
}

HRESULT Mesh3D::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
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

	s_shaderVariables.worldView = s_effect->GetVariableByName("g_mWorldView")->AsMatrix();
	s_shaderVariables.worldViewIT = s_effect->GetVariableByName("g_mWorldViewIT")->AsMatrix();
	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
	s_shaderVariables.enableFlatShading = s_effect->GetVariableByName("g_bEnableFlatShading")->AsScalar();
	s_shaderVariables.color = s_effect->GetVariableByName("g_vColor")->AsVector();

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_inputLayout));

	return S_OK;
}

void Mesh3D::releaseShader()
{
	SAFE_RELEASE(s_inputLayout);
	SAFE_RELEASE(s_effect);
}


void Mesh3D::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (s_effect == nullptr)
	{
		return;
	}

	XMMATRIX worldView = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view);
	XMMATRIX proj = XMLoadFloat4x4(&projection);

	s_shaderVariables.worldView->SetMatrix(reinterpret_cast<float*>(worldView.r));
	s_shaderVariables.worldViewIT->SetMatrix(reinterpret_cast<float*>(XMMatrixTranspose(XMMatrixInverse(nullptr, worldView)).r));
	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldView * proj).r));


	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion, 3 floats normal
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void Mesh3D::setShaderVariables(bool flatShading, PackedVector::XMCOLOR col)
{
	s_shaderVariables.enableFlatShading->SetBool(flatShading);
	s_shaderVariables.color->SetFloatVector(PackedVector::XMLoadColor(&col).m128_f32); // XMLoadColor normalizes color [0-255] -> [0.0-1.0]
}

Mesh3D::ShaderVariables::ShaderVariables()
	: worldView(nullptr),
	worldViewIT(nullptr),
	worldViewProj(nullptr),
	enableFlatShading(nullptr),
	color(nullptr)
{
}

Mesh3D::ShaderVariables::~ShaderVariables()
{
}

bool Mesh3D::readObj(const std::string& path)
{
	// Load obj into standard vector container
	if (!ObjLoader::loadObj(path, m_vertexData, m_indexData))
		return false;

	float scale = ObjLoader::normalizeSize(m_vertexData);
	ObjLoader::calculateNormals(m_vertexData, m_indexData);

	return true;
}

ID3DX11Effect* Mesh3D::s_effect = nullptr;
Mesh3D::ShaderVariables Mesh3D::s_shaderVariables;
ID3D11InputLayout* Mesh3D::s_inputLayout = nullptr;