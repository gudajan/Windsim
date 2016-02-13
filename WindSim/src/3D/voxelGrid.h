#ifndef VOXEL_GRID_H
#define VOXEL_GRID_H

#include "object3D.h"
#include "simulator.h"
#include "common.h"

#include <DirectXMath.h>

#include <vector>
#include <thread>
#include <mutex>

class ObjectManager;
struct ID3D11UnorderedAccessView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture3D;
struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectVectorVariable;
struct ID3DX11EffectScalarVariable;
struct ID3DX11EffectUnorderedAccessViewVariable;
struct ID3DX11EffectShaderResourceVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;
class Logger;

enum VoxelType : char
{
	CELL_TYPE_FLUID = 0,
	CELL_TYPE_INFLOW, // 1
	CELL_TYPE_OUTFLOW, // 2
	CELL_TYPE_SOLID_SLIP, // 3
	CELL_TYPE_SOLID_NO_SLIP, // 4
	CELL_TYPE_SOLID_BOUNDARY //5
};

class VoxelGrid : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	VoxelGrid(ObjectManager* manager, DirectX::XMUINT3 resolution, DirectX::XMFLOAT3 voxelSize, const std::string& simulator, Logger* logger);
	//VoxelGrid(VoxelGrid&& other);
	~VoxelGrid();

	HRESULT create(ID3D11Device* device, bool clearClientBuffers = false) override; // Create custom viewport, renderTargets, UAV etc
	void release() override;

	// Iterate mesh objects and render/voxelize into voxelGrid
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;

	DirectX::XMUINT3 getResolution() const { return m_resolution; };
	DirectX::XMFLOAT3 getVoxelSize() const { return m_voxelSize; };

	bool resize(DirectX::XMUINT3 resolution, DirectX::XMFLOAT3 voxelSize);
	void setVoxelize(bool voxelize) { m_voxelize = voxelize; };
	void setRenderVoxel(bool renderVoxel) { m_renderVoxel = renderVoxel; };
	void setRenderGlyphs(bool renderGlyphs) { m_renderGlyphs = renderGlyphs; };
	void setGlyphSettings(Orientation orientation, float position);
	void setGlyphQuantity(const DirectX::XMUINT2& quantity);
	void setSimulator(const std::string& exe);
	void updateSimulation();

private:
	void createGridData(); // Create cube for line rendering
	void updateVelocity(ID3D11DeviceContext* context);

	void renderGridBox(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	void voxelize(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, bool copyStaging);
	void renderVoxel(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	void renderVelocity(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	void calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, double elapsedTime);

	struct ShaderVariables
	{
		ShaderVariables();
		ID3DX11EffectMatrixVariable* worldViewProj; // Object space -> world space -> Camera/View space -> projection space
		ID3DX11EffectMatrixVariable* objToWorld; // Mesh object space -> world space
		ID3DX11EffectMatrixVariable* gridToVoxel; // Grid Object Space -> Voxel Space (use position as 3D index into voxel grid texture)
		ID3DX11EffectMatrixVariable* worldToVoxel; // Combined matrix: world space -> grid object space -> voxel space
		ID3DX11EffectMatrixVariable* voxelProj; // Projection matrix for voxelization (orthogonal, aligned with the voxelgrid)
		ID3DX11EffectMatrixVariable* voxelWorldViewProj; // Voxel space -> grid object space -> world space -> camera/view space -> projection space
		ID3DX11EffectMatrixVariable* voxelWorldView; // Voxel space -> grid object space -> world space -> camera/view space -> projection space

		ID3DX11EffectVectorVariable* camPos;
		ID3DX11EffectVectorVariable* resolution;
		ID3DX11EffectVectorVariable* voxelSize;
		ID3DX11EffectUnorderedAccessViewVariable* gridUAV;
		ID3DX11EffectShaderResourceVariable* gridSRV;
		ID3DX11EffectUnorderedAccessViewVariable* gridAllUAV;
		ID3DX11EffectShaderResourceVariable* gridAllSRV;
		ID3DX11EffectShaderResourceVariable* velocityField;

		ID3DX11EffectScalarVariable* glyphOrientation;
		ID3DX11EffectVectorVariable* glyphQuantity;
		ID3DX11EffectScalarVariable* glyphPosition;
	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_meshInputLayout;
	static ID3D11InputLayout* s_gridInputLayout;

	ObjectManager* m_manager;

	DirectX::XMUINT3 m_resolution; // Resolution of the grid
	DirectX::XMFLOAT3 m_voxelSize; // Size of one voxel in object space of the grid
	DirectX::XMUINT2 m_glyphQuantity;

	bool m_reinit; // Indicates if voxelgrid has to be reinitialized, because resolution, voxelSize or simulator changed
	bool m_initSim; // Indicates that the simulation has to be initialized
	bool m_updateDimensions; // Indicates that the simulation dimensions have to be updated
	bool m_updateGrid; // Indicates that the simulation should be updated after next voxelization

	uint32_t m_cubeIndices;

	bool m_voxelize;
	int m_counter;
	bool m_renderVoxel;
	bool m_renderGlyphs;
	bool m_calculateDynamics;

	ID3D11Texture3D* m_gridTextureGPU; // Texture in GPU memory, filled in pixel shader
	ID3D11Texture3D* m_gridAllTextureGPU; // Texture, containing the voxelizations of all meshes
	ID3D11Texture3D* m_gridAllTextureStaging; // Texture in system memory: The data of gpu texture is copyied here and than may be accessed by the cpu
	ID3D11UnorderedAccessView* m_gridUAV; // UAV for filling the voxel grid
	ID3D11ShaderResourceView* m_gridSRV; // SRV for one voxelization, used in compute shader
	ID3D11UnorderedAccessView* m_gridAllUAV; // UAV for all Voxelizations
	ID3D11ShaderResourceView* m_gridAllSRV; // SRV for volume rendering
	ID3D11Texture3D* m_velocityTexture;
	ID3D11Texture3D* m_velocityTextureStaging;
	ID3D11ShaderResourceView* m_velocitySRV;

	Simulator m_simulator;
	std::thread m_simulatorThread;
	std::unique_lock<std::mutex> m_lock;

};
#endif