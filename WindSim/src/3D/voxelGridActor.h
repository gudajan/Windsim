#ifndef VOXEL_GRID_ACTOR_H
#define VOXEL_GRID_ACTOR_H

#include "actor.h"
#include "voxelGrid.h"

class VoxelGridActor : public Actor
{
public:
	VoxelGridActor(VoxelGrid& grid, int id, const std::string& name);

	VoxelGridActor* clone() override;
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;
	void renderVolume(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime);
	VoxelGrid* getObject() override { return &m_grid; };
	void reCenter(); // Recalculate position so the grid center is at the origin

private:
	VoxelGrid& m_grid;
};

#endif