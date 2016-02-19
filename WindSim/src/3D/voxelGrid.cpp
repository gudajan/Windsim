#include "voxelGrid.h"
#include "objectManager.h"
#include "meshActor.h"
#include "mesh3D.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"

#include <qelapsedtimer.h>

#include <mutex>
#include <future>

using namespace DirectX;

VoxelGrid::VoxelGrid(ObjectManager* manager, XMUINT3 resolution, XMFLOAT3 voxelSize, const std::string& simulator, Logger* logger)
	: Object3D(logger),
	m_manager(manager),
	m_resolution(resolution),
	m_voxelSize(voxelSize),
	m_glyphQuantity(32, 32),
	m_reinit(false),
	m_initSim(false),
	m_updateDimensions(false),
	m_updateGrid(false),
	m_cubeIndices(0),
	m_voxelize(true),
	m_counter(-1),
	m_renderVoxel(true),
	m_renderGlyphs(true),
	m_calculateDynamics(true),
	m_gridTextureGPU(nullptr),
	m_gridAllTextureGPU(nullptr),
	m_gridAllTextureStaging(nullptr),
	m_gridUAV(nullptr),
	m_gridSRV(nullptr),
	m_gridAllUAV(nullptr),
	m_gridAllSRV(nullptr),
	m_velocityTexture(nullptr),
	m_velocityTextureStaging(nullptr),
	m_velocitySRV(nullptr),
	m_simulator(simulator, logger),
	m_simulatorThread(&Simulator::loop, &m_simulator),
	m_lock(m_simulator.getVoxelGridMutex(), std::defer_lock)
{
	createGridData();
	m_simulator.postMessageToSim({ MsgToSim::StartProcess }); // Start simulator process
}

VoxelGrid::~VoxelGrid()
{
	m_simulator.postMessageToSim({ MsgToSim::Exit });
	m_simulatorThread.join(); // Wait until thread finished (and simulator process exited/terminated)
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
	s_shaderVariables.voxelWorldView = s_effect->GetVariableByName("g_mVoxelWorldView")->AsMatrix();

	s_shaderVariables.camPos = s_effect->GetVariableByName("g_vCamPos")->AsVector();
	s_shaderVariables.resolution = s_effect->GetVariableByName("g_vResolution")->AsVector();
	s_shaderVariables.voxelSize = s_effect->GetVariableByName("g_vVoxelSize")->AsVector();

	s_shaderVariables.glyphOrientation = s_effect->GetVariableByName("g_sGlyphOrientation")->AsScalar();
	s_shaderVariables.glyphQuantity = s_effect->GetVariableByName("g_vGlyphQuantity")->AsVector();
	s_shaderVariables.glyphPosition = s_effect->GetVariableByName("g_sGlyphPosition")->AsScalar();

	s_shaderVariables.gridUAV = s_effect->GetVariableByName("g_gridUAV")->AsUnorderedAccessView();
	s_shaderVariables.gridSRV = s_effect->GetVariableByName("g_gridSRV")->AsShaderResource();
	s_shaderVariables.gridAllUAV = s_effect->GetVariableByName("g_gridAllUAV")->AsUnorderedAccessView();
	s_shaderVariables.gridAllSRV = s_effect->GetVariableByName("g_gridAllSRV")->AsShaderResource();
	s_shaderVariables.velocityField = s_effect->GetVariableByName("g_velocitySRV")->AsShaderResource();

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

	Object3D::create(device, false); // Create vertex and index buffer for grid rendering and calls release

	// Create Texture3D for a grid, containing voxelization of one mesh, filled in the pixel shader
	D3D11_TEXTURE3D_DESC td = {};
	td.Width = m_resolution.x / 4;  // As we have to use 32 bit unsigned integer, simply encode 4 char values/voxel in one grid cell
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


	// Create velocity field textures
	// Use one staging texture for writing the velocities from CPU to GPU and use CopyResource to copy the staging texture to a GPU usable default texture

	// Default texture
	td.Width = m_resolution.x;
	td.Height = m_resolution.y;
	td.Depth = m_resolution.z;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 3D velocity vector per voxel
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0; // No CPU Access for this texture
	td.MiscFlags = 0;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_velocityTexture));
	V_RETURN(device->CreateShaderResourceView(m_velocityTexture, nullptr, &m_velocitySRV));

	// Staging texture
	td.Usage = D3D11_USAGE_STAGING;
	td.BindFlags = 0;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_velocityTextureStaging));

	// Delay simulation update by setting a flag. This way, the update/initialization occurs not until the voxelization was performed once.
	if (!m_simulator.isRunning())
		m_initSim = true;
	else
		m_updateDimensions = true;

	// Resize the grid
	{
		std::lock_guard<std::mutex> guard(m_simulator.getVoxelGridMutex());
		m_simulator.getCellGrid().resize(m_resolution.x / 4 * m_resolution.y * m_resolution.z, 0);
	}


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
	SAFE_RELEASE(m_velocityTexture);
	SAFE_RELEASE(m_velocityTextureStaging);
	SAFE_RELEASE(m_velocitySRV);
}

void VoxelGrid::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_reinit)
	{
		createGridData();
		create(device, false);
		m_reinit = false;
		m_counter = -1;
	}
	//OutputDebugStringA("FRAME\n");

	// Check for messages from simulator
	for( auto& msg : m_simulator.getAllMessagesFromSim())
	{
		if (msg->type == MsgToRenderer::UpdateVelocity && !m_initSim && !m_updateDimensions) // If dimensions are currently updated, skip velocity update
		{
			std::lock_guard<std::mutex> guard(m_simulator.getVelocityMutex());
			QElapsedTimer elapsed;
			elapsed.start();
			//OutputDebugStringA("Start copying velocity!\n");
			updateVelocity(context);
			OutputDebugStringA(("ELAPSED: velocity copy: " + std::to_string(elapsed.nsecsElapsed() / 1000000) + "msec\n").c_str());
			m_simulator.postMessageToSim({ MsgToSim::FinishedVelocityAccess });
		}
	}

	// Calculates dynamics motion of meshes, depending on the current velocity field
	if (m_calculateDynamics)
		calculateDynamics(device, context, world, elapsedTime);

	renderGridBox(device, context, world, view, projection);

	if (m_voxelize) // If voxelization issued
	{
		// Only copy to staging if last copy to CPU is finished and it is necessary (i.e. simulation must be updated somehow)
		// and the cellGrid is available
		if (m_counter == -1 && (m_updateGrid || m_updateDimensions || m_initSim) && m_lock.try_lock())
		{
			voxelize(device, context, world, true);
			m_counter = 0;
		}
		else
			voxelize(device, context, world, false);

		m_voxelize = false;
	}

	// Make sure, the cpu only accesses the voxel grid if the GPU copying is done, to avoid pipeline stalling ( GPU copying needs 2 frames)
	// Additionally only copy to cpu if former grid was written to shared memory and upgrad is necessary or simulation should be reinitialized
	if (m_counter >= 2)
	{
		//OutputDebugStringA(("Rendering\n"));

		uint32_t xRes = m_resolution.x / 4;

		// Get CPU Access
		D3D11_MAPPED_SUBRESOURCE msr;
		context->Map(m_gridAllTextureStaging, 0, D3D11_MAP_READ, 0, &msr); // data contains pointer to texture data
		if (msr.pData)
		{
			// The mapped data may include padding in the memory, because of GPU optimizations
			// This means a 10x10x10 texture may be mapped as a 16x16x16 block
			// The real pitches from one row/depth slice to another are given by the D3D11_MAPPED_SUBRESOURCE

			uint32_t* cells = static_cast<uint32_t*>(msr.pData);

			// Temps
			int paddedRowPitch = msr.RowPitch / sizeof(uint32_t); // convert byte size to block resolution
			int paddedDepthPitch = msr.DepthPitch / sizeof(uint32_t);
			int rowPitch = xRes;
			int depthPitch = xRes * m_resolution.y;

			std::vector<uint32_t>& grid = m_simulator.getCellGrid();
			// Extract the voxel grid from the padded texture block
			for (int z = 0; z < m_resolution.z; ++z)
			{
				for (int y = 0; y < m_resolution.y; ++y)
				{
					for (int x = 0; x < xRes; ++x)
					{
						grid[x + y * rowPitch + z * depthPitch] = cells[x + y * paddedRowPitch + z * paddedDepthPitch];
					}
				}
			}
		}
		// Remove CPU Access
		context->Unmap(m_gridAllTextureStaging, 0);

		// Unlock mutex which was locked on copying to staging
		m_lock.unlock();

		// Send delayed init/update message for simulation
		if (m_initSim || m_updateDimensions)
		{
			DimMsg msg;
			msg.res = { m_resolution.x, m_resolution.y, m_resolution.z };
			msg.vs = { m_voxelSize.x, m_voxelSize.y, m_voxelSize.z };
			msg.type = m_initSim ? DimMsg::InitSim : DimMsg::UpdateDimensions;
			m_simulator.postMessageToSim(msg);
			m_initSim = false;
			m_updateDimensions = false;
		}
		else if (m_updateGrid) // If objects were modified, and those modifications were already copied to the CPU
		{
			m_simulator.postMessageToSim({ MsgToSim::UpdateGrid });
			m_updateGrid = false;
		}

		m_counter = -1; // Voxelization cycle finished
	}

	if (m_renderVoxel)
		renderVoxel(device, context, world, view, projection);

	if (m_renderGlyphs)
		renderVelocity(device, context, world, view, projection);

	if (m_counter > -1)
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

void VoxelGrid::setGlyphSettings(Orientation orientation, float position)
{
	s_shaderVariables.glyphOrientation->SetInt(orientation);
	s_shaderVariables.glyphPosition->SetFloat(position);
}

void VoxelGrid::setGlyphQuantity(const XMUINT2& quantity)
{
	m_glyphQuantity = quantity;
	s_shaderVariables.glyphQuantity->SetIntVector(reinterpret_cast<int*>(&m_glyphQuantity));
}

void VoxelGrid::setSimulator(const std::string& cmdline)
{
	StrMsg msg;
	msg.type = StrMsg::SimulatorCmd;
	msg.str = cmdline;
	msg.res = StrMsg::Resolution{m_resolution.x, m_resolution.y, m_resolution.z};
	msg.vs = StrMsg::VoxelSize{m_voxelSize.x, m_voxelSize.y, m_voxelSize.z};
	m_simulator.postMessageToSim(msg);
}

void VoxelGrid::updateSimulation()
{
	// Only update the simulation if there is no reinitialization in process
	if (!m_reinit) m_updateGrid = true;
}

void VoxelGrid::createGridData()
{
	float x = m_resolution.x * m_voxelSize.x;
	float y = m_resolution.y * m_voxelSize.y;
	float z = m_resolution.z * m_voxelSize.z;

	m_vertexData =
	{
		x, y, 0.0f,
		x, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, y, 0.0f,
		x, y, z,
		x, 0.0f, z,
		0.0f, 0.0f, z,
		0.0f, y, z
	};

	m_indexData =
	{
		// Box lines
		0, 1,
		0, 3,
		0, 4,
		1, 2,
		1, 5,
		2, 3,
		2, 6,
		3, 7,
		4, 5,
		4, 7,
		5, 6,
		6, 7,
		// Box triangles
		0, 1, 3,
		4, 7, 6,
		0, 4, 5,
		1, 5, 2,
		2, 6, 3,
		4, 0, 3,
		1, 2, 3,
		5, 4, 6,
		1, 0, 5,
		5, 6, 2,
		6, 7, 3,
		7, 4, 3
	};
	m_numIndices = 12 * 2; // 12 lines with 2 vertices
	m_cubeIndices = 12 * 3; // 12 triangles with 3 vertices

}

void VoxelGrid::updateVelocity(ID3D11DeviceContext* context)
{
	// Map staging texture, write velocity field to it, unmap, copy resource to gpu
	D3D11_MAPPED_SUBRESOURCE msr;
	context->Map(m_velocityTextureStaging, 0, D3D11_MAP_WRITE, 0, &msr);
	if (msr.pData)
	{
		int paddedRowPitch = msr.RowPitch / sizeof(float);
		int paddedDepthPitch = msr.DepthPitch / sizeof(float);
		int rowPitch = m_resolution.x * 3; // three floats in original buffer
		int depthPitch = rowPitch * m_resolution.y;

		float* data = static_cast<float*>(msr.pData);
		float* velocity = m_simulator.getVelocity();

		auto fillVel = [data, velocity, paddedRowPitch, paddedDepthPitch, rowPitch, depthPitch, this](int zStart, int zWork)
		{
			for (int z = zStart; z < zStart+zWork; ++z)
			{
				for (int y = 0; y < m_resolution.y; ++y)
				{
					for (int x = 0; x < m_resolution.x; ++x)
					{
						for (int i = 0; i < 3; ++i)
						{
							data[(x * 4) + i + y * paddedRowPitch + z * paddedDepthPitch] = velocity[(x * 3) + i + y * rowPitch + z * depthPitch];
						}
						data[(x * 4) + 3 + y * paddedRowPitch + z * paddedDepthPitch] = 0;
					}
				}
			}
		};

		int sizeZ = m_resolution.z / 8;
		std::vector<std::future<void>> handles;
		handles.reserve(8);
		for (int i = 0; i < 8; ++i)
			handles.push_back(std::async(std::launch::async, fillVel, i * sizeZ, sizeZ));

		for (int i = 0; i < 8; ++i)
			handles[i].get();
	}
	context->Unmap(m_velocityTextureStaging, 0);

	context->CopyResource(m_velocityTexture, m_velocityTextureStaging);
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

void VoxelGrid::voxelize(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, bool copyStaging)
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
	// Disable old renderTarget to enable arbitrary viewport sizes (i.e. voxel grid sizes)
	// Otherwise the viewport size is clamped to the resolution of the current render target
	context->OMSetRenderTargets(0, nullptr, nullptr);

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
	XMMATRIX voxelizeX = XMMatrixRotationNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), degToRad(90)) * XMMatrixScaling(1.0f, 1.0f, -1.0f);

	// Create orthogonal projection aligned with voxel grid looking along x-axis
	XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0, m_resolution.z, 0, m_resolution.y, 0, m_resolution.x);

	// Compute dispatch numbers for compute shader
	// yz values are devided by 16 because of the numthreads attribute in the compute shader, x value is devided by 16 because 4 voxels are encoded in one cell and 4 threads are dispatched
	XMUINT3 dispatch(std::ceil(m_resolution.x / 16.0f), std::ceil(m_resolution.y / 16.0f), std::ceil((m_resolution.z / 16.0f)));

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
			s_shaderVariables.gridUAV->SetUnorderedAccessView(m_gridUAV);
			s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

			ID3D11Buffer* vb = ma->getMesh().getVertexBuffer();
			ID3D11Buffer* ib = ma->getMesh().getIndexBuffer();
			context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
			context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexed(ma->getMesh().getNumIndices(), 0, 0);

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

			context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

			// Remove shader resources
			s_shaderVariables.gridSRV->SetResource(nullptr);
			s_shaderVariables.gridAllUAV->SetUnorderedAccessView(nullptr);
			s_effect->GetTechniqueByIndex(0)->GetPassByName("Combine")->Apply(0, context);
		}
	}

	// ###################################
	// CELLTYPES
	// ###################################
	// Use compute shader to compute the specific cell types for each voxel
	s_shaderVariables.gridAllUAV->SetUnorderedAccessView(m_gridAllUAV);
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellType")->Apply(0, context);

	context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

	s_shaderVariables.gridAllUAV->SetUnorderedAccessView(nullptr);
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellType")->Apply(0, context);

	// Copy texture from GPU memory to system memory where it is accessable by the cpu
	if (copyStaging)
	{
		context->CopyResource(m_gridAllTextureStaging, m_gridAllTextureGPU);
	}

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
	XMMATRIX voxelWorldView = voxelToGrid * w * v;
	XMMATRIX voxelWorldViewProj = voxelWorldView * p;
	XMMATRIX viewInv = XMMatrixInverse(nullptr, v);
	XMMATRIX worldInv = XMMatrixInverse(nullptr, w);
	s_shaderVariables.voxelWorldViewProj->SetMatrix(reinterpret_cast<float*>(voxelWorldViewProj.r));
	s_shaderVariables.voxelWorldView->SetMatrix(reinterpret_cast<float*>(voxelWorldView.r));

	s_shaderVariables.objToWorld->SetMatrix(reinterpret_cast<float*>(voxelToGrid.r));

	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<int*>(&m_resolution));

	//Calculate camera position in Voxel Space (Texture space of grid texture -> [0, resolution])
	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, viewInv); // World Space
	camPos = XMVector3Transform(camPos, worldInv); // Grid Object Space
	camPos = XMVector3Transform(camPos, gridToVoxel); // Voxel Space
	s_shaderVariables.camPos->SetFloatVector(camPos.m128_f32);

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

void VoxelGrid::renderVelocity(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	XMMATRIX w = XMLoadFloat4x4(&world);
	XMMATRIX v = XMLoadFloat4x4(&view);
	XMMATRIX p = XMLoadFloat4x4(&projection);
	XMMATRIX worldViewProj = w * v * p;
	s_shaderVariables.worldViewProj->SetMatrix(reinterpret_cast<float*>((worldViewProj).r));

	XMMATRIX viewInv = XMMatrixInverse(nullptr, v);
	XMMATRIX worldInv = XMMatrixInverse(nullptr, w);

	XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // In camera space
	camPos = XMVector3Transform(camPos, viewInv); // World Space
	camPos = XMVector3Transform(camPos, worldInv); // Grid Object Space
	s_shaderVariables.camPos->SetFloatVector(camPos.m128_f32);

	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<int*>(&m_resolution));
	s_shaderVariables.voxelSize->SetFloatVector(reinterpret_cast<float*>(&m_voxelSize));

	s_shaderVariables.velocityField->SetResource(m_velocitySRV);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("VelocityGlyph")->Apply(0, context);

	UINT stride = 0;
	UINT offset = 0;
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->IASetVertexBuffers(0, 0, nullptr, &stride, &offset);
	context->Draw(m_glyphQuantity.x * m_glyphQuantity.y, 0);

	s_shaderVariables.velocityField->SetResource(nullptr);

	s_effect->GetTechniqueByIndex(0)->GetPassByName("VelocityGlyph")->Apply(0, context);
}

void VoxelGrid::calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, double elapsedTime)
{
	XMFLOAT4X4 worldToVoxelTex;
	// inverse(voxel grid texture space -> voxel grid object space -> world space)
	XMStoreFloat4x4(&worldToVoxelTex, XMMatrixInverse(nullptr, XMMatrixScalingFromVector(XMLoadUInt3(&m_resolution) * XMLoadFloat3(&m_voxelSize)) * XMLoadFloat4x4(&world)));

	for (auto& act : m_manager->getActors())
	{
		if (act.second->getType() == ObjectType::Mesh)
		{
			std::shared_ptr<MeshActor> ma = std::dynamic_pointer_cast<MeshActor>(act.second);
			ma->calculateDynamics(device, context, worldToVoxelTex, m_resolution, m_velocitySRV, elapsedTime);
		}
	}
}

VoxelGrid::ShaderVariables::ShaderVariables()
	: worldViewProj(nullptr),
	objToWorld(nullptr),
	gridToVoxel(nullptr),
	worldToVoxel(nullptr),
	voxelProj(nullptr),
	voxelWorldViewProj(nullptr),
	voxelWorldView(nullptr),
	camPos(nullptr),
	resolution(nullptr),
	gridUAV(nullptr),
	gridAllUAV(nullptr),
	gridAllSRV(nullptr),
	velocityField(nullptr),
	glyphOrientation(nullptr),
	glyphQuantity(nullptr),
	glyphPosition(nullptr)
{
}

ID3DX11Effect* VoxelGrid::s_effect = nullptr;
VoxelGrid::ShaderVariables VoxelGrid::s_shaderVariables;
ID3D11InputLayout* VoxelGrid::s_meshInputLayout = nullptr;
ID3D11InputLayout* VoxelGrid::s_gridInputLayout = nullptr;