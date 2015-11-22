#include "meshActor.h"
#include "mesh.h"
#include "settings.h"

using namespace DirectX;

MeshActor::MeshActor(Mesh& mesh)
	: Actor(),
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
		m_mesh.setShaderVariables(m_flatShading, m_color);
		m_mesh.render(device, context, getWorld(), view, projection);
	}
}