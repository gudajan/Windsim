#include "object3D.h"
#include "common.h"
#include "intersection.h"
#include "objLoader.h"
#include "dx11renderer.h"

#include <d3d11.h>

using namespace DirectX;

Object3D::Object3D(DX11Renderer* renderer)
	: m_vertexBuffer(nullptr),
	m_indexBuffer(nullptr),
	m_numIndices(0),
	m_vertexData(),
	m_indexData(),
	m_renderer(renderer)
{
}

Object3D::Object3D(Object3D&& other)
	: m_vertexBuffer(other.m_vertexBuffer),
	m_indexBuffer(other.m_indexBuffer),
	m_numIndices(other.m_numIndices),
	m_vertexData(std::move(other.m_vertexData)),
	m_indexData(std::move(other.m_indexData)),
	m_renderer(std::move(other.m_renderer))
{
}

Object3D::~Object3D()
{
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
	bd.ByteWidth = static_cast<UINT>(sizeof(float) * m_vertexData.size());
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

	bd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_indexData.size());
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

// Assuming the vertex layout is: { Position (3*float), Normal (3*float) }
bool Object3D::intersect(XMFLOAT3& origin, XMFLOAT3& direction, float& distance) const
{
	return Geometry::intersect(origin, direction, m_vertexData, m_indexData, 6, distance);
}

void Object3D::getBoundingBox(XMFLOAT3& center, XMFLOAT3& extends)
{
	std::vector<float> c;
	std::vector<float> e;
	ObjLoader::findBoundingBox(m_vertexData, c, e);

	center = { c[0], c[1], c[2] };
	extends = { e[0], e[1], e[2] };
}

void Object3D::log(const std::string& msg)
{
	m_renderer->getLogger()->logit(QString::fromStdString(msg));
}


