#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"
#include "marker.h"
#include "mesh3D.h"
#include "dynamics.h"
#include <DirectXPackedVector.h>


struct ID3D11ShaderResourceView;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh3D& mesh, int id, const std::string& name);

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
	Mesh3D* getObject() override { return &m_mesh; };
	void calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& worldToVoxelTex, const DirectX::XMUINT3& texResolution, const DirectX::XMFLOAT3& voxelSize, ID3D11ShaderResourceView* field, double elapsedTime);
	void updateCalcRotation() { m_dynamics.updateCalcRotation(); };
	const DirectX::XMFLOAT3 getAngularVelocity() const;
	const DirectX::XMFLOAT3 getCenterOfMass() const;

	bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float& distance) const override;

	void computeWorld() override;

	Mesh3D& getMesh() { return m_mesh; };

	const DirectX::XMFLOAT4X4& getDynWorld() const { return m_calcDynamics ? m_dynRenderWorld : m_world; };
	bool getVoxelize() const { return m_voxelize; };
	void setVoxelize(bool voxelize) { m_voxelize = voxelize; };
	void setDynamics(bool dynamics) { m_calcDynamics = dynamics; m_dynamics.reset(); };
	void resetDynamics() { m_dynamics.reset(); };
	void setDensity(float density) { m_density = density; };
	void setLocalRotationAxis(const DirectX::XMFLOAT3& axis) { m_dynamics.setRotationAxis(axis); m_dynamics.reset(); };
	void setShowAccelArrow(bool showAccelArrow) { m_showAccelArrow = showAccelArrow; };
	void setSimRunning(bool simRunning) { m_simRunning = simRunning; };
	void updateInertiaTensor();

private:
	Marker m_marker;
	Mesh3D& m_mesh;
	Dynamics m_dynamics;

	DirectX::XMFLOAT4X4 m_dynRenderWorld;
	DirectX::XMFLOAT4X4 m_dynCalcWorld;
	DirectX::BoundingBox m_boundingBox;

	bool m_flatShading;
	DirectX::PackedVector::XMCOLOR m_color;
	bool m_voxelize;
	bool m_calcDynamics;
	bool m_simRunning;
	bool m_showAccelArrow;
	float m_density;

	bool m_hovered; // The mouse is hovering above the mesh
	bool m_selected; // The mesh is the currently selected object
	bool m_modified; // The mesh is currently modified (scaled, translated or rotated)
};


#endif