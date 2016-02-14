#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"
#include "marker.h"
#include "dynamics.h"
#include <DirectXPackedVector.h>

class Mesh3D;
struct ID3D11ShaderResourceView;

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
	void setModified(bool modified) { m_modified = modified; };

	void create(ID3D11Device* device);
	void release();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;
	void calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& worldToVoxelTex, const DirectX::XMUINT3& texResolution, ID3D11ShaderResourceView* velocityField, double elapsedTime);

	bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float& distance) const override;

	Mesh3D& getMesh() { return m_mesh; };

	bool getVoxelize() const { return m_voxelize; };
	void setVoxelize(bool voxelize) { m_voxelize = voxelize; };
	void setDynamics(bool dynamics) { m_calcDynamics = dynamics; m_dynamics.reset(); };
	void setDensity(float density) { m_density = density; };
	void setLocalRotationAxis(const DirectX::XMFLOAT3& axis) { m_dynamics.setRotationAxis(axis); };
	void updateInertiaTensor();

private:
	Marker m_marker;
	Mesh3D& m_mesh;
	Dynamics m_dynamics;

	DirectX::XMFLOAT4X4 m_dynWorld;
	DirectX::BoundingBox m_boundingBox;

	bool m_flatShading;
	DirectX::PackedVector::XMCOLOR m_color;
	bool m_voxelize;
	bool m_calcDynamics;
	float m_density;

	bool m_hovered; // The mouse is hovering above the mesh
	bool m_selected; // The mesh is the currently selected object
	bool m_modified; // The mesh is currently modified (scaled, translated or rotated)
};


#endif