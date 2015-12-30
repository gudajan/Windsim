#ifndef VOXEL_GRID_ACTOR_H
#define VOXEL_GRID_ACTOR_H

#include "actor.h"

class VoxelGrid;

class VoxelGridActor : public Actor
{
public:
	VoxelGridActor(VoxelGrid& grid, int id);

	VoxelGridActor* clone() override;
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	void reCenter(); // Recalculate position so the grid center is at the origin
	void resize(DirectX::XMUINT3 resolution, DirectX::XMFLOAT3 voxelSize);
	void voxelize();
	void setRenderVoxel(bool renderVoxel);
	void setSimulator(const std::string& exe);

private:
	VoxelGrid& m_grid;
};

#endif