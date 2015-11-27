#include "meshActor.h"
#include "mesh3D.h"
#include "settings.h"

using namespace DirectX;

MeshActor::MeshActor(Mesh3D& mesh, int id)
	: Actor(ObjectType::Mesh, id),
	m_mesh(mesh),
	m_boundingBox(),
	m_flatShading(true),
	m_color(PackedVector::XMCOLOR(conf.mesh.dc.r / 255.0f, conf.mesh.dc.g / 255.0f, conf.mesh.dc.b / 255.0f, 1.0f)), // XMCOLOR constructor multiplies by 255.0f and packs color into one uint32_t
	m_hovered(false),
	m_selected(false)
{
	XMFLOAT3 center;
	XMFLOAT3 extends;
	mesh.getBoundingBox(center, extends);
	setBoundingBox(center, extends);
}

MeshActor::~MeshActor()
{
}

void MeshActor::setBoundingBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extends)
{
	m_boundingBox = BoundingBox(center, extends);
}

void MeshActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (m_render)
	{
		if (m_hovered)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.mesh.hc.r / 255.0f, conf.mesh.hc.g / 255.0f, conf.mesh.hc.b / 255.0f, 1.0f));
		else if (m_selected)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.gen.sc.r / 255.0f, conf.gen.sc.g / 255.0f, conf.gen.sc.b / 255.0f, 1.0f));
		else
			m_mesh.setShaderVariables(m_flatShading, m_color);
		m_mesh.render(device, context, getWorld(), view, projection);
	}
}

bool MeshActor::intersect(XMFLOAT3 origin, XMFLOAT3 direction, float& distance) const
{
	// Transform ray from world to object space
	XMVECTOR ori = XMLoadFloat3(&origin);
	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVectorSetW(dir, 0.0f);

	XMMATRIX world = XMLoadFloat4x4(&getWorld());

	XMMATRIX invWorld = XMMatrixInverse(nullptr, world);

	ori = XMVector3Transform(ori, invWorld);
	dir = XMVector4Transform(dir, invWorld);
	dir = XMVector3Normalize(dir);

	XMFLOAT3 o;
	XMFLOAT3 d;
	XMStoreFloat3(&o, ori);
	XMStoreFloat3(&d, dir);

	// Check if bounding box is intersected
	BoundingBox transformedBB;
	m_boundingBox.Transform(transformedBB, world);
	float dist = 0;
	if (!m_boundingBox.Intersects(ori, dir, dist))
		return false;

	if (!m_mesh.intersect(o, d, distance))
		return false;

	// Transform distance from object to world space (just scaling has influence on distance)
	XMVECTOR  distVec = XMVector3Transform(dir * distance, XMMatrixScalingFromVector(XMLoadFloat3(&m_scale)));
	XMStoreFloat(&distance, XMVector3Length(distVec));
	return true;
}