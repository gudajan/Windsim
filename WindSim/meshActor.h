#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"
#include <DirectXPackedVector.h>

class Mesh3D;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh3D& mesh);
	~MeshActor();

	void setFlatShading(bool flat) { m_flatShading = flat; };
	void setColor(DirectX::PackedVector::XMCOLOR col) { m_color = col; };
	void setHovered(bool hovered) { m_hovered = hovered; };
	void setSelected(bool selected) { m_selected = selected; };

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, DirectX::XMFLOAT3& intersection) const override;

private:
	Mesh3D& m_mesh;

	bool m_flatShading;
	DirectX::PackedVector::XMCOLOR m_color;

	bool m_hovered; // The mouse is hovering above the mesh
	bool m_selected; // The mesh is the currently selected object
};


#endif