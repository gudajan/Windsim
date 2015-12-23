#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"
#include "marker.h"
#include <DirectXPackedVector.h>

class Mesh3D;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh3D& mesh, int id);

	MeshActor* clone() override;

	void setBoundingBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extends);
	void setFlatShading(bool flat) { m_flatShading = flat; };
	void setColor(DirectX::PackedVector::XMCOLOR col) { m_color = col; };
	void setHovered(bool hovered) { m_hovered = hovered; };
	void setSelected(bool selected) { m_selected = selected; };

	void create(ID3D11Device* device);
	void release();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float& distance) const override;

	Mesh3D& getMesh() { return m_mesh; };

	bool getVoxelize() const { return m_voxelize; };
	void setVoxelize(bool voxelize) { m_voxelize = voxelize; };

private:
	Marker m_marker;
	Mesh3D& m_mesh;
	DirectX::BoundingBox m_boundingBox;


	bool m_flatShading;
	DirectX::PackedVector::XMCOLOR m_color;
	bool m_voxelize;

	bool m_hovered; // The mouse is hovering above the mesh
	bool m_selected; // The mesh is the currently selected object
};


#endif