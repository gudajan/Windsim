#include "voxelGrid.h"
#include "objectManager.h"
#include "meshActor.h"
#include "mesh3D.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"

#include <qelapsedtimer.h>

using namespace DirectX;

VoxelGrid::VoxelGrid(ObjectManager* manager, XMUINT3 resolution, XMFLOAT3 voxelSize, const std::string& simulator, Logger* logger)
	: Object3D(logger),
	m_manager(manager),
	m_resolution(resolution),
	m_voxelSize(voxelSize),
	m_reinit(false),
	m_cubeIndices(0),
	m_voxelize(true),
	m_counter(0),
	m_renderVoxel(true),
	m_grid(),
	m_tempCells(),
	m_gridTextureGPU(nullptr),
	m_gridAllTextureGPU(nullptr),
	m_gridAllTextureStaging(nullptr),
	m_gridUAV(nullptr),
	m_gridSRV(nullptr),
	m_gridAllUAV(nullptr),
	m_gridAllSRV(nullptr),
	m_simulator(simulator, logger)
{
	createGridData();
}

VoxelGrid::VoxelGrid(VoxelGrid&& other)
	: Object3D(std::move(other)),
	m_manager(other.m_manager),
	m_resolution(std::move(other.m_resolution)),
	m_voxelSize(std::move(other.m_voxelSize)),
	m_reinit(other.m_reinit),
	m_cubeIndices(other.m_cubeIndices),
	m_voxelize(other.m_voxelize),
	m_counter(other.m_counter),
	m_renderVoxel(other.m_renderVoxel),
	m_grid(std::move(other.m_grid)),
	m_gridTextureGPU(other.m_gridTextureGPU),
	m_gridAllTextureGPU(other.m_gridAllTextureGPU),
	m_gridAllTextureStaging(other.m_gridAllTextureStaging),
	m_gridUAV(other.m_gridUAV),
	m_gridSRV(other.m_gridSRV),
	m_gridAllUAV(other.m_gridAllUAV),
	m_gridAllSRV(other.m_gridAllSRV),
	m_simulator(std::move(other.m_simulator))
{
}


HRESULT VoxelGrid::createShaderFromFile(const std::wstring& shaderPath, ID3D11Device* device, const bool reload)
{
	HRESULT hr;

	releaseShader();

	if (reload)
	{
		ID3DBlob* blob;
		if (D3DX11CompileEffectFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, D3DCOMPILE_DEBUG, 0, device, &s_effect, &blob) != S_OK)
		{
			char* buffer = reinterpret_cast<char*>(blob->GetBufferPointer());
			OutputDebugStringA(buffer);
			return E_FAIL;
		}
	}
	else
	{
		V_RETURN(D3DX11CreateEffectFromFile(shaderPath.c_str(), 0, device, &s_effect));
	}

	s_shaderVariables.worldViewProj = s_effect->GetVariableByName("g_mWorldViewProj")->AsMatrix();
	s_shaderVariables.objToWorld = s_effect->GetVariableByName("g_mObjToWorld")->AsMatrix();
	s_shaderVariables.gridToVoxel = s_effect->GetVariableByName("g_mGridToVoxel")->AsMatrix();
	s_shaderVariables.worldToVoxel = s_effect->GetVariableByName("g_mWorldToVoxel")->AsMatrix();
	s_shaderVariables.voxelProj = s_effect->GetVariableByName("g_mVoxelProj")->AsMatrix();
	s_shaderVariables.voxelWorldViewProj = s_effect->GetVariableByName("g_mVoxelWorldViewProj")->AsMatrix();

	s_shaderVariables.camPosVS = s_effect->GetVariableByName("g_vCamPosVS")->AsVector();
	s_shaderVariables.resolution = s_effect->GetVariableByName("g_vResolution")->AsVector();
	s_shaderVariables.voxelSize = s_effect->GetVariableByName("g_vVoxelSize")->AsVector();

	s_shaderVariables.gridUAV = s_effect->GetVariableByName("g_gridUAV")->AsUnorderedAccessView();
	s_shaderVariables.gridSRV = s_effect->GetVariableByName("g_gridSRV")->AsShaderResource();
	s_shaderVariables.gridAllUAV = s_effect->GetVariableByName("g_gridAllUAV")->AsUnorderedAccessView();
	s_shaderVariables.gridAllSRV = s_effect->GetVariableByName("g_gridAllSRV")->AsShaderResource();

	// The same layout as meshes, as only they are voxelized
	D3D11_INPUT_ELEMENT_DESC meshLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA }
	};

	D3DX11_PASS_DESC pd;
	s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(meshLayout, sizeof(meshLayout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_meshInputLayout));

	D3D11_INPUT_ELEMENT_DESC gridLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA },
	};

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderGridBox")->GetDesc(&pd);

	V_RETURN(device->CreateInputLayout(gridLayout, sizeof(gridLayout) / sizeof(D3D11_INPUT_ELEMENT_DESC), pd.pIAInputSignature, pd.IAInputSignatureSize, &s_gridInputLayout));

	return S_OK;
}

void VoxelGrid::releaseShader()
{
	SAFE_RELEASE(s_meshInputLayout);
	SAFE_RELEASE(s_gridInputLayout);
	SAFE_RELEASE(s_effect);
}

HRESULT VoxelGrid::create(ID3D11Device* device, bool clearClientBuffers)
{
	HRESULT hr;

	release();

	Object3D::create(device, false); // Create vertex and index buffer for grid rendering

	// Resize the grid
	m_grid.resize(m_resolution.x * m_resolution.y * m_resolution.z);
	std::fill(m_grid.begin(), m_grid.end(), 0);
	m_tempCells.resize(m_resolution.x / 32 * m_resolution.y * m_resolution.z);
	std::fill(m_tempCells.begin(), m_tempCells.end(), 0);

	// Create Texture3D for a grid, containing voxelization of one mesh, filled in the pixel shader
	D3D11_TEXTURE3D_DESC td = {};
	td.Width = m_resolution.x / 32;  // As we have to use 32 bit unsigned integer, simply encode 32 bool values/voxel in one grid cell
	td.Height = m_resolution.y;
	td.Depth = m_resolution.z;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R32_UINT; // Atomic bitwise operations in shader (InterlockedXor) on UAVs are only supported for R32_SINT and R32_UINT
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0; // No CPU Access for this texture
	td.MiscFlags = 0;

	V_RETURN(device->CreateTexture3D(&td, nullptr , &m_gridTextureGPU));

	// Create Unordered Access View for this grid
	V_RETURN(device->CreateUnorderedAccessView(m_gridTextureGPU, nullptr, &m_gridUAV));

	// Create Shader Resource View for the single mesh voxelization grid
	V_RETURN(device->CreateShaderResourceView(m_gridTextureGPU, nullptr, &m_gridSRV));

	// Create the same texture again for containing all voxelizations
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_gridAllTextureGPU));

	// Create another UAV for the new texture
	V_RETURN(device->CreateUnorderedAccessView(m_gridAllTextureGPU, nullptr, &m_gridAllUAV));

	// Create Shader Resource View for combined grid
	V_RETURN(device->CreateShaderResourceView(m_gridAllTextureGPU, nullptr, &m_gridAllSRV));

	// Now create a texture in system memory, which is used by the GPU to copy the texture from the GPU memory to system memory
	td.Usage = D3D11_USAGE_STAGING; // GPU Read/Write and GPU to CPU copiing -> texture in system memory
	td.BindFlags = 0; // No binding to graphics pipeline allowed (and not needed)
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_gridAllTextureStaging));

	// Start simulator process
	m_simulator.start();

	return S_OK;
}

void VoxelGrid::release()
{
	Object3D::release();
	SAFE_RELEASE(m_gridTextureGPU);
	SAFE_RELEASE(m_gridAllTextureGPU);
	SAFE_RELEASE(m_gridAllTextureStaging);
	SAFE_RELEASE(m_gridUAV);
	SAFE_RELEASE(m_gridSRV);
	SAFE_RELEASE(m_gridAllUAV);
	SAFE_RELEASE(m_gridAllSRV);

	// Stop simulator process
	m_simulator.stop();
}

void VoxelGrid::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	const static int counterStart = 0;
	const static int counterCopyCPUParts = 4;
	const static int counterCopyCPUEnd = counterStart + counterCopyCPUParts;
	const static int counterVoxelize = counterCopyCPUEnd - 1; // Voxelization performed during last copy frame, wait frame for setting boundary cells
	const static int counterCopyGPU = counterVoxelize + 2;
	if (m_reinit)
	{
		createGridData();
		create(device, false);
		m_reinit = false;
	}

	renderGridBox(device, context, world, view, projection);

	if (m_voxelize && m_counter > counterCopyGPU) // Restart voxelization cycle if indicated by renderer AND if prior cycle was finished
	{
		m_counter = counterStart; // Start counting
		m_voxelize = false;
	}

	if (m_counter == counterVoxelize)
		voxelize(device, context, world);

	// Make sure, the cpu only accesses the voxel grid if the GPU copiing is done, to avoid pipeline stalling ( GPU copiing needs 1 frame)
	if (m_counter == counterCopyGPU)
	{
		uint32_t xRes = m_resolution.x / 32;

		// Get CPU Access
		D3D11_MAPPED_SUBRESOURCE msr;
		context->Map(m_gridAllTextureStaging, 0, D3D11_MAP_READ, 0, &msr); // data contains pointer to texture data
		if (msr.pData)
		{
			// The mapped data may include padding in the memory, because of GPU optimizations
			// This means a 10x10x10 texture may be mapped as a 16x16x16 block
			// The real pitches from one row/depth slice to another are givem by the D3D11_MAPPED_SUBRESOURCE

			uint32_t* cells = reinterpret_cast<uint32_t*>(msr.pData);

			// Temps
			int paddedRowPitch = msr.RowPitch / sizeof(uint32_t); // convert byte size to block resolution
			int paddedDepthPitch = msr.DepthPitch / sizeof(uint32_t);
			int rowPitch = xRes;
			int depthPitch = xRes * m_resolution.y;
			// Extract the voxel grid from the padded texture block
			for (int z = 0; z < m_resolution.z; ++z)
			{
				for (int y = 0; y < m_resolution.y; ++y)
				{
					for (int x = 0; x < xRes; ++x)
					{
						m_tempCells[x + y * rowPitch + z * depthPitch] = cells[x + y * paddedRowPitch + z * paddedDepthPitch];
					}
				}
			}
		}
		// Remove CPU Access
		context->Unmap(m_gridAllTextureStaging, 0);
	}

	// Copy the data into own grid for the next few frames
	// As the next few frames until the next voxelization are not so busy,
	// span this expensive operation over several frames to ensure a smoother frame rate instead of alternating fast and slow frames
	// THIS IS REALLY SLOW: TODO: Maybe perform in concurrent thread/process
	if (m_counter >= counterStart && m_counter < counterCopyCPUEnd)
	{
		int part = m_counter - counterStart; // Determine the part of the grid, which is copied this frame
		uint32_t partZSize = m_resolution.z / counterCopyCPUParts; // Depth (z-value) of one part
		uint32_t partZStart = partZSize * part; // Starting Depth of current part

		// Copy data into own grid and decode 32 bit cells into voxels
		QElapsedTimer timer;
		timer.start();

		uint32_t xRes = m_resolution.x / 32;
		uint32_t slice = xRes * m_resolution.y;
		uint32_t realSlice = m_resolution.x * m_resolution.y;
		uint32_t cell = 0;
		uint32_t index = 0;
		char val = CELL_TYPE_FLUID;
		for (int z = partZStart; z < partZStart + partZSize; ++z)
		{
			for (int y = 0; y < m_resolution.y; ++y)
			{
				for (int x = 0; x < xRes; ++x)
				{
					cell = m_tempCells[x + y * xRes + z * slice]; // Index in encoded grid
					index = x * 32 + y * m_resolution.x + z * realSlice; // Index in decoded grid
					for (int j = index; j < index + 32; ++j)
					{
						if (static_cast<int>(cell & (1 << j) > 0)) val = CELL_TYPE_SOLID_NO_SLIP;
						else val = CELL_TYPE_FLUID;
						m_grid[j] = val; // indicates if j-th bit of i-th cell is set
					}
				}
			}
		}
	}

	// Set final types of boundary cells for the simulation
	// performed within one frame
	else if (m_counter == counterCopyCPUEnd)
	{
		uint32_t realSlice = m_resolution.x * m_resolution.y;
		for (int y = 1; y < m_resolution.y - 1; y++)
		{
			for (int z = 1; z < m_resolution.z - 1; z++)
			{
				m_grid[0 + y * m_resolution.x + z * realSlice] = CELL_TYPE_INFLOW;
				m_grid[m_resolution.x - 1 + y * m_resolution.x + z * realSlice] = CELL_TYPE_OUTFLOW;
			}
		}

		for (int x = 0; x < m_resolution.x; x++)
		{
			for (int y = 0; y < m_resolution.y; y++)
			{
				m_grid[x + y * m_resolution.x + 0] = CELL_TYPE_SOLID_SLIP;
				m_grid[x + y * m_resolution.x + (m_resolution.z - 1) * realSlice] = CELL_TYPE_SOLID_SLIP;
			}
		}
	}

	if (m_renderVoxel)
		renderVoxel(device, context, world, view, projection);

	m_counter++;
}

bool VoxelGrid::resize(XMUINT3 resolution, XMFLOAT3 voxelSize)
{
	XMVECTOR currentRes = XMLoadUInt3(&m_resolution);
	XMVECTOR newRes = XMLoadUInt3(&resolution);
	XMVECTOR currentVS = XMLoadFloat3(&m_voxelSize);
	XMVECTOR newVS = XMLoadFloat3(&voxelSize);

	bool resEqual = XMVector3EqualInt(currentRes, newRes);
	bool vsEqual = XMVector3Equal(currentVS, newVS);

	if (resEqual && vsEqual)
		return false;

	m_resolution = resolution;
	m_voxelSize = voxelSize;
	m_reinit = true; // Cannot resize here, because we have no dx11 device
	return true;
}

void VoxelGrid::setSimulator(const std::string& exe)
{
	if(m_simulator.setExecutable(exe))
		m_reinit = true;
}

void VoxelGrid::createGridData()
{
	float x = m_resolution.x * m_voxelSize.x;
	float y = m_resolution.y * m_voxelSize.y;
	float z = m_resolution.z * m_voxelSize.z;

	m_vertexData =
	{
		0.0f, 0.0f, 0.0f, // left, lower, front
		0.0f, 0.0f, z, // left, lower, back
		0.0f, y, 0.0f, // left, upper, front
		0.0f, y, z, // left, upper, back
		x, 0.0f, 0.0f, // right, lower, front
		x, 0.0f, z, // right, lower, back
		x, y, 0.0f, // right, upper, front
		x, y, z // right, upper, back
	};

	m_indexData =
	{
		// Box lines
		0, 1,
		0, 2,
		0, 4,
		1, 3,
		1, 5,
		2, 3,
		2, 6,
		3, 7,
		4, 5,
		4, 6,
		5, 7,
		6, 7,
		// Box triangles
		0, 1, 2,
		0, 2, 4,
		0, 5, 1,
		0, 4, 5,
		1, 3, 2,
		1, 5, 3,
		2, 6, 4,
		2, 3, 6,
		3, 7, 6,
		3, 5, 7,
		4, 6, 5,
		5, 6 ,7
	};
	m_numIndices = 12 * 2; // 12 lines with 2 vertices
	m_cubeIndices = 12 * 3; // 12 triangles with 3 vertices

}

void VoxelGrid::renderGridBox(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	XMMATRIX worldViewProj = XMLoadFloat4x4(&world) * XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);

	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldViewProj).r));

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderGridBox")->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_gridInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->DrawIndexed(m_numIndices, 0, 0);
}

void VoxelGrid::voxelize(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world)
{
	// The voxelization is performed along the x-axis, so the 32 bit cells are properly aligned
	// -> The viewport is aligned with the yz side of the voxel grid
	// -> The orthogonal projection is aligned along the x-axis (e.g. x-Resolution corresponds to zFar)
	// -> As the voxelization in shader is always performed along the z-axis, rotate the voxelized objects about 90 degrees
	//    Additionally, the grid has to be mirrored at the xy-plane (because for the grid access index, x and z values are switched;
	//    a 90 deg rotation in 2D is x1 = -x2, x2 = x1; so one of the axises has to be mirrored)
	// -> On accessing the grid in the pixel shader (voxelGrid.fx -> psVoxelize()), the x and z values a switched (rotation and mirroring)

	// Save old renderTarget
	ID3D11RenderTargetView* tempRTV = nullptr;
	ID3D11DepthStencilView* tempDSV = nullptr;
	context->OMGetRenderTargets(1, &tempRTV, &tempDSV);

	// Save old viewport
	D3D11_VIEWPORT tempVP[1];
	UINT vpCount = 1;
	context->RSGetViewports(&vpCount, tempVP);

	// Set viewport, so projection plane resolution matches grid resolution
	// -> The generated fragments have the same size as the voxel sizes
	// If we increase the resolution, the voxelization is gets more conservative (more (barely) touched voxels are set) as more rays are casted per voxel row
	float factor = 1.0f;
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = static_cast<float>(m_resolution.z * factor);
	vp.Height = static_cast<float>(m_resolution.y * factor);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// Clear combined grid UAV once each voxelization
	const UINT iniVals[] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewUint(m_gridAllUAV, iniVals);

	// Passed projection and view transformations of current camera are ignored
	// Transform mesh into "Voxel Space" (coordinates should be in range [0, resolution] so we can use floor(position) in pixel shader for accessing the grid)
	// World to Grid Object Space:
	XMMATRIX worldToGrid = XMMatrixInverse(nullptr, XMLoadFloat4x4(&world));
	// Grid Object Space to Voxel Space
	XMMATRIX gridToVoxel = XMMatrixScalingFromVector(XMVectorReciprocal(XMLoadFloat3(&m_voxelSize))); // Scale with 1 / voxelSize so position is index into grid
	// Perform rotation and mirroring to enforce voxelization along x-instead of z-axis
	XMMATRIX voxelizeX = XMMatrixRotationNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), degToRad(90)) * XMMatrixScaling(1.0f, 1.0f, -1.0f);;

	// Create orthogonal projection aligned with voxel grid looking along x-axis
	XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0, m_resolution.z, 0, m_resolution.y, 0, m_resolution.x);

	s_shaderVariables.worldToVoxel->SetMatrix(reinterpret_cast<float*>((worldToGrid * gridToVoxel * voxelizeX).r));
	s_shaderVariables.voxelProj->SetMatrix(reinterpret_cast<float*>(proj.r));
	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<int*>(&m_resolution));
	s_shaderVariables.voxelSize->SetFloatVector(reinterpret_cast<float*>(&m_voxelSize));

	const unsigned int strides[] = { sizeof(float) * 6 }; // 3 floats postion, 3 floats normal for mesh layout
	const unsigned int offsets[] = { 0 };
	context->IASetInputLayout(s_meshInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (const auto& act : m_manager->getActors())
	{
		// Only meshes and enabled objects are voxelized
		if (act.second->getType() == ObjectType::Mesh)
		{
			std::shared_ptr<MeshActor> ma = std::dynamic_pointer_cast<MeshActor>(act.second);

			if (!ma->getVoxelize())
				continue;

			// ###################################
			// VOXELIZATION
			// ###################################

			// Clear UAV for each mesh
			context->ClearUnorderedAccessViewUint(m_gridUAV, iniVals);

			// Mesh Object Space -> World Space:
			XMMATRIX objToWorld = XMLoadFloat4x4(&ma->getWorld());
			s_shaderVariables.objToWorld->SetMatrix(reinterpret_cast<float*>(objToWorld.r));
			// Set Unordered Access View
			s_shaderVariables.gridUAV->SetUnorderedAccessView(m_gridUAV);

			s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

			ID3D11Buffer* vb = ma->getMesh().getVertexBuffer();
			ID3D11Buffer* ib = ma->getMesh().getIndexBuffer();
			context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
			context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexed(ma->getMesh().getNumIndices(), 0, 0);

			// Remove Unordered Access View
			s_shaderVariables.gridUAV->SetUnorderedAccessView(nullptr);

			s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

			// ###################################
			// COMBINE
			// ###################################
			// Use compute shader to combine this meshes voxelization with the former ones:
			// Set shader resources
			s_shaderVariables.gridSRV->SetResource(m_gridSRV);
			s_shaderVariables.gridAllUAV->SetUnorderedAccessView(m_gridAllUAV);

			s_effect->GetTechniqueByIndex(0)->GetPassByName("Combine")->Apply(0, context);

			// yz values are devided by 32 because of the numthreads attribute in the compute shader, x value is devided because 32 voxels are encoded in one cell
			XMUINT3 dispatch( std::ceil(m_resolution.x / 32.0f), std::ceil(m_resolution.y / 32.0f), std::ceil((m_resolution.z / 32.0f)) );

			context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

			// Remove shader resources
			s_shaderVariables.gridSRV->SetResource(nullptr);
			s_shaderVariables.gridAllUAV->SetUnorderedAccessView(nullptr);

			s_effect->GetTechniqueByIndex(0)->GetPassByName("Combine")->Apply(0, context);
		}
	}

	// Copy texture from GPU memory to system memory where it is accessable by the cpu
	context->CopyResource(m_gridAllTextureStaging, m_gridAllTextureGPU);

	// Restore old render targets and viewport
	context->OMSetRenderTargets(1, &tempRTV, tempDSV);

	context->RSSetViewports(1, tempVP);

	SAFE_RELEASE(tempRTV);
	SAFE_RELEASE(tempDSV);
}

void VoxelGrid::renderVoxel(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	XMMATRIX w = XMLoadFloat4x4(&world);
	XMMATRIX v = XMLoadFloat4x4(&view);
	XMMATRIX p = XMLoadFloat4x4(&projection);
	XMMATRIX worldViewProj = w * v * p;
	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldViewProj).r));

	XMMATRIX gridToVoxel = XMMatrixScalingFromVector(XMVectorReciprocal(XMLoadFloat3(&m_voxelSize)));
	s_shaderVariables.gridToVoxel->SetMatrix(reinterpret_cast<float*>((gridToVoxel).r));

	XMMATRIX voxelToGrid = XMMatrixScalingFromVector(XMLoadFloat3(&m_voxelSize));
	XMMATRIX voxelWorldViewProj = voxelToGrid * w * v * p;
	s_shaderVariables.voxelWorldViewProj->SetMatrix(reinterpret_cast<float*>(voxelWorldViewProj.r));

	s_shaderVariables.objToWorld->SetMatrix(reinterpret_cast<float*>(voxelToGrid.r));

	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<int*>(&m_resolution));

	//Calculate camera position in Voxel Space (Texture space of grid texture -> [0, resolution])
	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, v)); // World Space
	camPos = XMVector3Transform(camPos, XMMatrixInverse(nullptr, w)); // Grid Object Space
	camPos = XMVector3Transform(camPos, gridToVoxel); // Voxel Space
	s_shaderVariables.camPosVS->SetFloatVector(camPos.m128_f32);

	s_shaderVariables.gridAllSRV->SetResource(m_gridAllSRV);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderVoxel")->Apply(0, context);

	const unsigned int strides[] = { sizeof(float) * 3 }; // 3 floats postion
	const unsigned int offsets[] = { 0 };
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, strides, offsets);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(s_gridInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_cubeIndices, m_numIndices, 0);

	s_shaderVariables.gridAllSRV->SetResource(nullptr);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("RenderVoxel")->Apply(0, context);

}

VoxelGrid::ShaderVariables::ShaderVariables()
	: worldViewProj(nullptr),
	objToWorld(nullptr),
	gridToVoxel(nullptr),
	worldToVoxel(nullptr),
	voxelProj(nullptr),
	voxelWorldViewProj(nullptr),
	camPosVS(nullptr),
	resolution(nullptr),
	gridUAV(nullptr),
	gridAllUAV(nullptr),
	gridAllSRV(nullptr)
{
}

ID3DX11Effect* VoxelGrid::s_effect = nullptr;
VoxelGrid::ShaderVariables VoxelGrid::s_shaderVariables;
ID3D11InputLayout* VoxelGrid::s_meshInputLayout = nullptr;
ID3D11InputLayout* VoxelGrid::s_gridInputLayout = nullptr;