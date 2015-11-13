#include "meshActor.h"
#include "mesh.h"

MeshActor::MeshActor(Mesh& mesh)
	: Actor(),
	m_mesh(mesh)
{
}

MeshActor::~MeshActor()
{
}

void MeshActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (m_render)
	{
		m_mesh.render(device, context, getWorld(), view, projection);
	}
}