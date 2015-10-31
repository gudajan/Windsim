#include "object3D.h"
#include "objLoader.h"

#include "d3dx11effect.h"
#include <d3d11.h>
#include <d3dcompiler.h>

using namespace DirectX;

Object3D::Object3D(const std::string& path)
	: m_vertexBuffer(nullptr),
	m_indexBuffer(nullptr),
	m_numIndices(0),
	m_vertexData(),
	m_indexData()
{
	if (!readObj(path))
	{
		throw(std::runtime_error("Could not load obj-file '" + path + "' into 3D object!"));
	}
	m_numIndices = m_indexData.size() * 3; // 1 Triangle consists of 3 indices
}

Object3D::Object3D(Object3D&& other)
	: m_vertexBuffer(other.m_vertexBuffer),
	m_indexBuffer(other.m_indexBuffer),
	m_numIndices(other.m_numIndices),
	m_vertexData(std::move(other.m_vertexData)),
	m_indexData(std::move(other.m_indexData))
{
}

Object3D::~Object3D()
{
}

HRESULT Object3D::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
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

	s_shaderVariables.worldView = s_effect->GetVariableByName("g_worldView")->AsMatrix();
	s_shaderVariables.worldViewIT = s_effect->GetVariableByName("g_worldViewIT")->AsMatrix();
	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_worldViewProj")->AsMatrix();

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

void Object3D::releaseShader()
{
	SAFE_RELEASE(s_inputLayout);
	SAFE_RELEASE(s_effect);
}


HRESULT Object3D::create(ID3D11Device* device, bool clearClientBuffers)
{
	HRESULT hr;

	release();

	if (m_vertexData.empty() || m_indexData.empty())
	{
		// TODO: maybe use HRESULT instead of exception?
		throw std::runtime_error("no vertex or index data available");
	}

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * m_vertexData.size());
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = m_vertexData.data();

	V_RETURN(device->CreateBuffer(&bd, &initData, &m_vertexBuffer));
	if (clearClientBuffers)
	{
		m_vertexData.clear();
		m_vertexData.shrink_to_fit();
	}

	bd.ByteWidth = static_cast<UINT>(sizeof(Triangle) * m_indexData.size());
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	initData.pSysMem = m_indexData.data();

	V_RETURN(device->CreateBuffer(&bd, &initData, &m_indexBuffer));
	if (clearClientBuffers)
	{
		m_indexData.clear();
		m_indexData.shrink_to_fit();
	}

	return S_OK;
}

void Object3D::release()
{
	SAFE_RELEASE(m_indexBuffer);
	SAFE_RELEASE(m_vertexBuffer);
}

void Object3D::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world)
{
	if (s_effect == nullptr)
	{
		return;
	}

	//const XMMATRIX worldView = world * view;
	//s_shaderVariables.worldView->SetMatrix((float*)worldView.r);
	XMMATRIX worldView = XMLoadFloat4x4(&world);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, 4 / 3, 0.1f, 100.0f);

	s_shaderVariables.worldView->SetMatrix(reinterpret_cast<float*>(worldView.r));
	s_shaderVariables.worldViewIT->SetMatrix(reinterpret_cast<float*>(XMMatrixTranspose(XMMatrixInverse(nullptr, worldView)).r));
	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldView * proj).r));

	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);

	const unsigned int strides[] = { sizeof(Vertex) };
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

Object3D::ShaderVariables::ShaderVariables()
	: worldView(nullptr),
	worldViewIT(nullptr),
	worldViewProj(nullptr)
{
}

Object3D::ShaderVariables::~ShaderVariables()
{
}

bool Object3D::readObj(const std::string& path)
{
	// Load obj into standard vector container
	std::vector<float> vertex;
	std::vector<uint32_t> index;
	if (!ObjLoader::loadObj(path, vertex, index))
	{
		return false;
	}

	float scale = ObjLoader::normalizeSize(vertex);
	ObjLoader::calculateNormals(vertex, index);

	// Copy standard vertex vector container into custom vertex vector container
	int factor = sizeof(Vertex) / sizeof(float);
	int vertexSize = vertex.size() / factor;
	m_vertexData.clear();
	m_vertexData.reserve(vertexSize);
	for (int i = 0; i < vertexSize; ++i)
	{
		XMFLOAT3 p;
		XMStoreFloat3(&p, XMVectorSet(vertex[i * factor], vertex[i * factor + 1], vertex[i * factor + 2], 0));
		XMFLOAT3 n;
		XMStoreFloat3(&n, XMVectorSet(vertex[i * factor + 3], vertex[i * factor + 4], vertex[i * factor + 5], 0));
		m_vertexData.push_back({ p, n });
	}

	// Copy standard index vector container into custom index vector container
	factor = sizeof(Triangle) / sizeof(uint32_t);
	int indexSize = index.size() / factor;
	m_indexData.clear();
	m_indexData.reserve(indexSize);
	for (int i = 0; i < indexSize; ++i)
	{
		m_indexData.push_back({ index[i * factor], index[i * factor + 1], index[i * factor + 2] });
	}
	return true;
}

ID3DX11Effect* Object3D::s_effect = nullptr;
Object3D::ShaderVariables Object3D::s_shaderVariables;
ID3D11InputLayout* Object3D::s_inputLayout = nullptr;

//void Object3D::renderTestQuad(ID3D11Device* device, ID3D11DeviceContext* context)
//{
//	context->IASetInputLayout(s_inputLayout);
//	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//	unsigned int stride = sizeof(Vertex);
//	unsigned int offset = 0;
//
//	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, 4 / 3, 0.1f, 100.0f);
//
//
//	context->IASetVertexBuffers(0, 1, &m_testQuadVB, &stride, &offset);
//	context->IASetIndexBuffer(m_testQuadIB, DXGI_FORMAT_R32_UINT, 0);
//
//	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<const float*>(&proj));
//
//	s_effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);
//
//	context->DrawIndexed(6, 0, 0);
//}

//void Object3D::createTestQuadGeometryBuffers(ID3D11Device* device, ID3D11DeviceContext* context)
//{
//	float magenta[] = { 1.0f, 0.0f, 1.0f};
//	float blue[] = { 0.0f, 0.0f, 1.0f };
//
//	Vertex vertices[] =
//	{
//		{ XMFLOAT3(-1.0f, -1.0f, 10.5f), XMFLOAT3(magenta) },
//		{ XMFLOAT3(-1.0f, 1.0f, 10.5f), XMFLOAT3(blue) },
//		{ XMFLOAT3(1.0f, 1.0f, 10.5f), XMFLOAT3(blue) },
//		{ XMFLOAT3(+1.0f, -1.0f, 10.5f), XMFLOAT3(magenta) },
//	};
//
//	D3D11_BUFFER_DESC vbDesc;
//	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
//	vbDesc.ByteWidth = sizeof(Vertex) * 4;
//	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//	vbDesc.CPUAccessFlags = 0;
//	vbDesc.MiscFlags = 0;
//	D3D11_SUBRESOURCE_DATA vertexInitData;
//	vertexInitData.pSysMem = vertices;
//	device->CreateBuffer(&vbDesc, &vertexInitData, &m_testQuadVB);
//
//	unsigned int indices[] =
//	{
//		0, 1, 2,
//		0, 2, 3
//	};
//
//	D3D11_BUFFER_DESC ibDesc;
//	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
//	ibDesc.ByteWidth = sizeof(UINT) * 6;
//	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
//	ibDesc.CPUAccessFlags = 0;
//	ibDesc.MiscFlags = 0;
//	D3D11_SUBRESOURCE_DATA indexInitData;
//	indexInitData.pSysMem = indices;
//	device->CreateBuffer(&ibDesc, &indexInitData, &m_testQuadIB);
//}