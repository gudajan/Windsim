#ifndef VOXEL_GRID_H
#define VOXEL_GRID_H

#include "object3D.h"

#include <DirectXMath.h>

#include <vector>

class ObjectManager;
struct ID3D11UnorderedAccessView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture3D;
struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectVectorVariable;
struct ID3DX11EffectUnorderedAccessViewVariable;
struct ID3DX11EffectShaderResourceVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;


class VoxelGrid : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	VoxelGrid(ObjectManager* manager, DirectX::XMUINT3 resolution, DirectX::XMFLOAT3 voxelSize);

	HRESULT create(ID3D11Device* device, bool clearClientBuffers = false) override; // Create custom viewport, renderTargets, UAV etc
	void release() override;

	// Iterate mesh objects and render/voxelize into voxelGrid
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	DirectX::XMUINT3 getResolution() const { return m_resolution; };
	DirectX::XMFLOAT3 getVoxelSize() const { return m_voxelSize; };

	bool resize(DirectX::XMUINT3 resolution, DirectX::XMFLOAT3 voxelSize);
	void setVoxelize(bool voxelize) { m_voxelize = voxelize; };

private:
	void createGridData(); // Create cube for line rendering
	void renderGridBox(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	void voxelize(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world);
	void renderVoxel(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);

	struct ShaderVariables
	{
		ShaderVariables();
		ID3DX11EffectMatrixVariable* worldViewProj; // Object space -> world space -> Camera/View space -> projection space
		ID3DX11EffectMatrixVariable* objToWorld; // Mesh object space -> world space
		ID3DX11EffectMatrixVariable* gridToVoxel; // Grid Object Space -> Voxel Space (use position as 3D index into voxel grid texture)
		ID3DX11EffectMatrixVariable* worldToVoxel; // Combined matrix: world space -> grid object space -> voxel space
		ID3DX11EffectMatrixVariable* voxelProj; // Projection matrix for voxelization (orthogonal, aligned with the voxelgrid)
		ID3DX11EffectMatrixVariable* voxelWorldViewProj; // Voxel space -> grid object space -> world space -> camera/view space -> projection space

		ID3DX11EffectVectorVariable* camPosVS;
		ID3DX11EffectVectorVariable* resolution;
		ID3DX11EffectVectorVariable* voxelSize;
		ID3DX11EffectUnorderedAccessViewVariable* gridUAV;
		ID3DX11EffectShaderResourceVariable* gridSRV;

	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_meshInputLayout;
	static ID3D11InputLayout* s_gridInputLayout;

	ObjectManager* m_manager;

	DirectX::XMUINT3 m_resolution; // Resolution of the grid
	DirectX::XMFLOAT3 m_voxelSize; // Size of one voxel in object space of the grid
	bool m_resize; // Indicates if voxelgrid has to be resized, because resolution or voxelSize changed
	uint32_t m_cubeIndices;

	bool m_voxelize;
	int m_counter;

	std::vector<unsigned char> m_grid;
	ID3D11Texture3D* m_gridTextureGPU; // Texture in GPU memory, filled in pixel shader
	ID3D11Texture3D* m_gridTextureStaging; // Texture in system memory: The data of gpu texture is copyied here and than may be accessed by the cpu
	ID3D11UnorderedAccessView* m_gridUAV; // UAV for filling the voxel grid
	ID3D11ShaderResourceView* m_gridSRV; // SRV for volume rendering


};
#endif