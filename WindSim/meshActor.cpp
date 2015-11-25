#include "meshActor.h"
#include "mesh3D.h"
#include "settings.h"

using namespace DirectX;

MeshActor::MeshActor(Mesh3D& mesh)
	: Actor(ObjectType::Mesh),
	m_mesh(mesh),
	m_flatShading(true),
	m_color(PackedVector::XMCOLOR(conf.mesh.dc.r / 255.0f, conf.mesh.dc.g / 255.0, conf.mesh.dc.b / 255.0, 1.0f)) // XMCOLOR constructor multiplies by 255.0f and packs color into one uint32_t
{
}

MeshActor::~MeshActor()
{
}

void MeshActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (m_render)
	{
		if (m_hovered)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.mesh.hc.r / 255.0f, conf.mesh.hc.g / 255.0, conf.mesh.hc.b / 255.0, 1.0f));
		else if (m_selected)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.gen.sc.r / 255.0f, conf.gen.sc.g / 255.0, conf.gen.sc.b / 255.0, 1.0f));
		else
			m_mesh.setShaderVariables(m_flatShading, m_color);
		m_mesh.render(device, context, getWorld(), view, projection);
	}
}

bool MeshActor::intersect(XMFLOAT3 origin, XMFLOAT3 direction, XMFLOAT3& intersection) const
{
	// Transform ray from world to object space
	XMVECTOR ori = XMLoadFloat3(&origin);
	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVectorSetW(dir, 0.0f);

	XMMATRIX invModel = XMMatrixInverse(nullptr, XMLoadFloat4x4(&getWorld()));

	ori = XMVector3Transform(ori, invModel);
	dir = XMVector4Transform(dir, invModel);
	dir = XMVector3Normalize(dir);

	XMFLOAT3 o;
	XMFLOAT3 d;
	XMStoreFloat3(&o, ori);
	XMStoreFloat3(&d, dir);

	return m_mesh.intersect(o, d, intersection);
}