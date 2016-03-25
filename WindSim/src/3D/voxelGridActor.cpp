#include "voxelGridActor.h"
#include "voxelGrid.h"

using namespace DirectX;

VoxelGridActor::VoxelGridActor(VoxelGrid& grid, int id)
	: Actor(ObjectType::VoxelGrid, id),
	m_grid(grid)
{
	//reCenter(); // Center the grid initially
}

VoxelGridActor* VoxelGridActor::clone()
{
	return new VoxelGridActor(*this);
}

void VoxelGridActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_render)
	{
		m_grid.render(device, context, m_world, view, projection, elapsedTime);
	}
}

void VoxelGridActor::renderWindTunnel(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_render)
	{
		m_grid.renderWindTunnel(device, context, m_world, view, projection, elapsedTime);
	}
}

void VoxelGridActor::reCenter()
{
	// Origin of voxel grid is at x = y = z = 0 (viewing along +z axis -> origin is at left, lower, back of grid)
	XMVECTOR res = XMLoadUInt3(&m_grid.getResolution());
	XMVECTOR vSize = XMLoadFloat3(&m_grid.getVoxelSize());

	// Calc world pos of center
	XMVECTOR center = XMVector3Transform(res * vSize * 0.5, XMLoadFloat4x4(&m_world));

	// Shift position about - center
	XMStoreFloat3(&m_pos, XMLoadFloat3(&m_pos) - center);
	computeWorld();
}

void VoxelGridActor::resize(XMUINT3 resolution, XMFLOAT3 voxelSize)
{
	m_grid.resize(resolution, voxelSize);
}

void VoxelGridActor::voxelize()
{
	m_grid.setVoxelize(true);
}

void VoxelGridActor::setRenderVoxel(bool renderVoxel)
{
	m_grid.setRenderVoxel(renderVoxel);
}

void VoxelGridActor::setGlyphSettings(bool render, Orientation orientation, float position, const XMUINT2& quantity)
{
	m_grid.setRenderGlyphs(render);
	m_grid.setGlyphSettings(orientation, position);
	m_grid.setGlyphQuantity(quantity);
}