#include "voxelGrid.h"
#include "objectManager.h"
#include "meshActor.h"
#include "mesh3D.h"
#include "depthStencil.h"
#include "dx11renderer.h"
#include "settings.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3dx11effect.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QPropertyAnimation>

#include <mutex>
#include <future>
#include <sstream>

using namespace DirectX;

VoxelGrid::VoxelGrid(ObjectManager* manager, const QString& windTunnelSettings, XMUINT3 resolution, XMFLOAT3 voxelSize, DX11Renderer* renderer, QObject* parent)
	: QObject(parent),
	Object3D(renderer),
	m_manager(manager),
	m_resolution(resolution),
	m_voxelSize(voxelSize),
	m_voxelType(Solid),
	m_glyphQuantity(32, 32),
	m_glyphOrientation(XY_PLANE),
	m_glyphPosition(0.5),
	m_updateGrid(true),
	m_processSimResults(false),
	m_simAvailable(true),
	m_dynamicsCounter(-1),
	m_voxelizationCounter(-1),
	m_cubeIndices(0),
	m_voxelize(true),
	m_renderVoxel(false),
	m_renderGlyphs(false),
	m_calculateDynamics(true),
	m_conservative(true),
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
	m_pressureTexture(nullptr),
	m_pressureTextureStaging(nullptr),
	m_pressureSRV(nullptr),
	m_wtRenderer(windTunnelSettings.toStdString()),
	m_wtSettings(windTunnelSettings),
	m_lastMod(QFileInfo(windTunnelSettings).lastModified()),
	m_volumeRenderer(),
	m_simulator(windTunnelSettings, resolution, voxelSize, m_renderer),
	m_simulationThread()
{
	createGridData();

	m_simulator.moveToThread(&m_simulationThread);

	// Start/stop thread
	//connect(&m_simulationThread, &QThread::started, &m_simulator, &Simulator::start);
	connect(this, &VoxelGrid::startSimulation, &m_simulator, &Simulator::start);
	connect(this, &VoxelGrid::stopSimulation, &m_simulator, &Simulator::stop);
	connect(this, &VoxelGrid::pauseSimulation, &m_simulator, &Simulator::pause);
	//connect(&m_simulationThread, &QThread::finished, &m_simulator, &QObject::deleteLater); // Error in delete of windtunnel
	connect(&m_simulationThread, &QThread::finished, &m_simulationThread, &QObject::deleteLater);

	// Grid -> Simulation/WindTunnel
	connect(this, &VoxelGrid::gridUpdated, &m_simulator, &Simulator::updateGrid);
	connect(this, &VoxelGrid::gridResized, &m_simulator, &Simulator::setGridDimension);
	connect(this, &VoxelGrid::resetSimulation, &m_simulator, &Simulator::resetSimulation);
	// Gui settings
	connect(this, &VoxelGrid::simSettingsChanged, &m_simulator, &Simulator::changeSimSettings);
	connect(this, &VoxelGrid::smokeSettingsChanged, &m_simulator, &Simulator::changeSmokeSettings);
	connect(this, &VoxelGrid::lineSettingsChanged, &m_simulator, &Simulator::changeLineSettings);

	// Simulation/WindTunnel -> Grid
	connect(&m_simulator, &Simulator::stepDone, this, &VoxelGrid::processSimResult);
	connect(&m_simulator, &Simulator::simUpdated, this, &VoxelGrid::enableGridUpdate);
	connect(&m_simulator, &Simulator::simulatorReady, this, &VoxelGrid::simulatorReady);

	m_simulationThread.start();
}

VoxelGrid::~VoxelGrid()
{
	emit stopSimulation();
	m_simulator.continueSim(true);
	m_simulationThread.wait(); // Wait until simulation thread finished
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

QJsonObject VoxelGrid::getVoxelSettingsDefault()
{
	return QJsonObject{ { "enabled", false }, { "type", Solid } };
}

QJsonObject VoxelGrid::getGlyphSettingsDefault(XMUINT3 resolution)
{
	QJsonObject tmp{ { "x", static_cast<int>(resolution.x) }, { "y", static_cast<int>(resolution.y) } };
	QJsonObject pos{ { "XY", 0.5 }, { "XZ", 0.5 }, { "YZ", 0.5 } };
	return QJsonObject{ { "enabled", false }, { "orientation", XY_PLANE }, { "position", pos }, { "quantity", tmp } };
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
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 3D velocity vector per voxel; Copying/Mapping performance is MUCH better for RGBA instead of RGB (~4ms vs ~190ms)
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

	// Create pressure field textures (equivalent to velcity field)
	// Default texture
	td.Width = m_resolution.x;
	td.Height = m_resolution.y;
	td.Depth = m_resolution.z;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R32_FLOAT; // one scalar per voxel
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = 0; // No CPU Access for this texture
	td.MiscFlags = 0;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_pressureTexture));
	V_RETURN(device->CreateShaderResourceView(m_pressureTexture, nullptr, &m_pressureSRV));

	// Staging texture
	td.Usage = D3D11_USAGE_STAGING;
	td.BindFlags = 0;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	V_RETURN(device->CreateTexture3D(&td, nullptr, &m_pressureTextureStaging));

	m_wtRenderer.init(device, m_resolution, m_voxelSize);

	m_volumeRenderer.create(device, m_resolution);

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
	SAFE_RELEASE(m_pressureTexture);
	SAFE_RELEASE(m_pressureTextureStaging);
	SAFE_RELEASE(m_pressureSRV);
	m_wtRenderer.release();
	m_volumeRenderer.release();
}

void VoxelGrid::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& world, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, double elapsedTime)
{
	//QElapsedTimer timer;
	//timer.start();

	// Process simulation results
	if (m_simAvailable && m_processSimResults && m_dynamicsCounter == -1)
	{
		QElapsedTimer t2;
		t2.start();
		updateVelocityPressure(context);
		OutputDebugStringA(("INFO: Update velocity lasted " + std::to_string(t2.nsecsElapsed() * 1e-6) + "msec\n").c_str());
		t2.restart();
		m_wtRenderer.updateDensity(context, m_simulator.getDensity(), m_simulator.getDensitySum());
		OutputDebugStringA(("INFO: Update density lasted " + std::to_string(t2.nsecsElapsed() * 1e-6) + "msec\n").c_str());
		t2.restart();
		m_wtRenderer.updateLines(context, m_simulator.getLines(), m_simulator.getReseedCounter(), m_simulator.getNumLines());
		m_processSimResults = false;
		OutputDebugStringA(("INFO: Update lines lasted " + std::to_string(t2.nsecsElapsed() * 1e-6) + "msec\n").c_str());
		m_simulator.continueSim();
		m_dynamicsCounter = 0;
	}
	// Calculates dynamics motion of meshes, depending on the current velocity field (wait 2 frames until the staging texture is copied to the GPU)
	if (m_dynamicsCounter >= 2)
	{
		calculateDynamics(device, context, world, elapsedTime);
		m_dynamicsCounter = -1;
	}
	if (m_dynamicsCounter > -1)
		m_dynamicsCounter++;

	// Voxelization
	if (m_voxelize)
	{
		// Only copy to staging if last copy to CPU is finished and grid update possible
		if (m_simAvailable && m_updateGrid && m_voxelizationCounter == -1)
		{
			voxelize(device, context, world, true);
			m_voxelizationCounter = 0;
		}
		else
			voxelize(device, context, world, false);

		m_voxelize = false;
	}
	// Make sure, the cpu only accesses the voxel grid if the GPU copying is done, to avoid pipeline stalling ( GPU copying needs 2 frames)
	// Additionally only copy to cpu if former grid was written to shared memory and upgrad is necessary or simulation should be reinitialized
	if (m_voxelizationCounter >= 2)
	{
		copyGrid(context);

		m_updateGrid = false;
		emit gridUpdated();

		m_voxelizationCounter = -1; // Voxelization cycle finished

		// When voxel grid of simulation is updated, reset the dynamic rotation, used for the dynamics calculation, to the current rotation of the mesh
		// This is needed to sample the velocity from appropriate positions (which correspond to the simulation) during the dynamics calculation
		for (auto it : m_manager->getActors())
		{
			if (it.second->getType() == ObjectType::Mesh)
				std::dynamic_pointer_cast<MeshActor>(it.second)->updateCalcRotation();
		}
	}
	if (m_voxelizationCounter > -1)
		m_voxelizationCounter++;

	renderGridBox(device, context, world, view, projection);

	if (m_renderVoxel)
		renderVoxel(device, context, world, view, projection);

	if (m_renderGlyphs)
		renderGlyphs(device, context, world, view, projection);

	//OutputDebugStringA(("INFO: VoxelGrid render lasted " + std::to_string(timer.nsecsElapsed() * 1e-6) + "msec\n").c_str());
}

void VoxelGrid::renderVolume(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime)
{

	DepthStencil::update(context);

	m_volumeRenderer.render(context, m_velocitySRV, DepthStencil::srv(), world, view, projection, m_resolution, m_voxelSize);

	XMFLOAT3 gridPos;
	XMVECTOR scale, rot, trans;
	XMMatrixDecompose(&scale, &rot, &trans, XMLoadFloat4x4(&world));
	XMStoreFloat3(&gridPos, trans);
	m_wtRenderer.render(device, context, DepthStencil::srv(), view, projection, gridPos, m_renderer->getCamera()->getCamPos(), m_renderer->getCamera()->getNearZ());

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

	// Recreate with new dimensions
	createGridData();
	create(m_renderer->getDevice(), false);

	emit gridResized(m_resolution, m_voxelSize);

	m_updateGrid = false;
	m_processSimResults = false;
	m_simAvailable = false;

	// Abort current render cycles
	m_voxelizationCounter = -1;
	m_dynamicsCounter = -1;
	m_simulator.continueSim(true); // Make sure the resize event is processed and further step events are not blocking

	return true;
}

void VoxelGrid::setVoxelSettings(const QJsonObject& settings)
{
	m_renderVoxel = settings["enabled"].toBool();
	m_voxelType = VoxelType(settings["type"].toInt());
}

void VoxelGrid::setGlyphSettings(const QJsonObject& settings)
{
	m_renderGlyphs = settings["enabled"].toBool();
	m_glyphOrientation = Orientation(settings["orientation"].toInt());
	QString key = m_glyphOrientation == XY_PLANE ? "XY" : m_glyphOrientation == XZ_PLANE ? "XZ" : "YZ";
	m_glyphPosition = settings["position"].toObject()[key].toDouble();
	QJsonObject json = settings["quantity"].toObject();
	m_glyphQuantity = XMUINT2(json["x"].toInt(), json["y"].toInt());
}

bool VoxelGrid::changeSimSettings(const QString& settingsFile)
{
	// Currently simulation settings file also contains rendering settings
	// -> Create new renderer with new file and initialize properly
	// Only recreate the wind tunnel if settings have changed
	QFileInfo info(settingsFile);
	QDateTime lastMod = info.lastModified();
	if (settingsFile == m_wtSettings && lastMod <= m_lastMod)
		return false;

	m_wtRenderer = wtl::WindTunnelRenderer(settingsFile.toStdString());
	m_wtRenderer.init(m_renderer->getDevice(), m_resolution, m_voxelSize);
	m_wtSettings = settingsFile;
	m_lastMod = lastMod;

	emit simSettingsChanged(settingsFile);

	m_updateGrid = false;
	m_processSimResults = false;
	m_simAvailable = false;

	// Abort current render cycles
	m_voxelizationCounter = -1;
	m_dynamicsCounter = -1;
	m_simulator.continueSim(true); // Make sure the settingsChange event is processed and further step events are not blocking

	return true;
};

void VoxelGrid::runSimulation(bool enabled)
{
	if (enabled)
	{
		emit startSimulation();
		for (auto& it: m_manager->getActors())
		{
			if (it.second->getType() == ObjectType::Mesh)
				std::dynamic_pointer_cast<MeshActor>(it.second)->setSimRunning(true);
		}
	}
	else
	{
		emit pauseSimulation();
		for (auto& it : m_manager->getActors())
		{
			if (it.second->getType() == ObjectType::Mesh)
				std::dynamic_pointer_cast<MeshActor>(it.second)->setSimRunning(false);
		}
	}
}

void VoxelGrid::changeSmokeSettings(const QJsonObject& settings)
{
	bool tmp = settings["enabled"].toBool();
	m_wtRenderer.smokeRendering(tmp ? wtl::Smoke::Enabled : wtl::Smoke::Disabled);
	emit smokeSettingsChanged(settings);
}

void VoxelGrid::changeLineSettings(const QJsonObject& settings)
{
	bool tmp = settings["enabled"].toBool();
	m_wtRenderer.lineRendering(tmp ? wtl::Line::Enabled : wtl::Line::Disabled);
	emit lineSettingsChanged(settings);
}

void VoxelGrid::changeVolumeSettings(const QJsonObject& settings)
{
	m_volumeRenderer.changeSettings(m_renderer->getDevice(), settings);
}

void VoxelGrid::restartSimulation()
{
	if (!changeSimSettings(m_wtSettings))
	{
		emit resetSimulation();
	}
}

void VoxelGrid::runSimulationSync(bool enabled)
{
	if (enabled)
	{
		emit startSimulation();
	}
	else
	{
		emit pauseSimulation();
		m_simulator.continueSim(true);

		m_processSimResults = false;
		m_updateGrid = false;
		m_simAvailable = false;

		m_voxelizationCounter = -1;
		m_dynamicsCounter = -1;

		// Block until simulation stopped running
		std::lock_guard<std::mutex> lock(m_simulator.getRunningMutex());
	}
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

void VoxelGrid::updateVelocityPressure(ID3D11DeviceContext* context)
{
	// Map staging texture, write velocity field to it, unmap, copy resource to gpu
	D3D11_MAPPED_SUBRESOURCE msr;
	context->Map(m_velocityTextureStaging, 0, D3D11_MAP_WRITE, 0, &msr);
	write3DTexture(&msr, m_simulator.getVelocity().data(), sizeof(float) * 4);
	context->Unmap(m_velocityTextureStaging, 0);

	context->CopyResource(m_velocityTexture, m_velocityTextureStaging);

	// Map staging texture, write pressure field to it, unmap, copy resource to gpu
	context->Map(m_pressureTextureStaging, 0, D3D11_MAP_WRITE, 0, &msr);
	write3DTexture(&msr, m_simulator.getPressure().data(), sizeof(float));
	context->Unmap(m_pressureTextureStaging, 0);

	context->CopyResource(m_pressureTexture, m_pressureTextureStaging);
}

void VoxelGrid::copyGrid(ID3D11DeviceContext* context)
{
	// Copy grid cell types
	std::vector<wtl::CellType>& cellTypes = m_simulator.getCellTypes();
	D3D11_MAPPED_SUBRESOURCE msr;
	context->Map(m_gridAllTextureStaging, 0, D3D11_MAP_READ, 0, &msr);
	read3DTexture(&msr, cellTypes.data(), sizeof(wtl::CellType));
	context->Unmap(m_gridAllTextureStaging, 0);
}

void VoxelGrid::read3DTexture(D3D11_MAPPED_SUBRESOURCE* msr, void* outData, int bytePerElem)
{
	if (!msr->pData || !outData)
		return;

	// The mapped data may include padding in the memory, because of GPU optimizations
	// This means a 10x10x10 texture may be mapped as a 16x16x16 block
	// The real pitches from one row/depth slice to another are given by the D3D11_MAPPED_SUBRESOURCE

	char* tex = static_cast<char*>(msr->pData);
	char* data = static_cast<char*>(outData);

	// Temps
	int paddedRowPitch = msr->RowPitch; // convert byte size to block resolution
	int paddedDepthPitch = msr->DepthPitch;
	int rowPitch = m_resolution.x * bytePerElem;
	int depthPitch = rowPitch * m_resolution.y;

	copyPadded3DTexture(data, rowPitch, depthPitch, tex, paddedRowPitch, paddedDepthPitch);
}

void VoxelGrid::write3DTexture(D3D11_MAPPED_SUBRESOURCE* msr, const void* inData, int bytePerElem)
{
	if (!msr->pData || !inData)
		return;

	char* tex = static_cast<char*>(msr->pData);
	const char* data = static_cast<const char*>(inData);

	// Temps
	int paddedRowPitch = msr->RowPitch; // convert byte size to block resolution
	int paddedDepthPitch = msr->DepthPitch;
	int rowPitch = m_resolution.x * bytePerElem;
	int depthPitch = rowPitch * m_resolution.y;

	copyPadded3DTexture(tex, paddedRowPitch, paddedDepthPitch, data, rowPitch, depthPitch);
}

void VoxelGrid::copyPadded3DTexture(char* outData, int outRowPitch, int outDepthPitch, const char* inData, int inRowPitch, int inDepthPitch)
{
	// Extract the voxel grid from the padded texture block
	if (outRowPitch == inRowPitch && outDepthPitch == inDepthPitch)
		std::memcpy(outData, inData, inDepthPitch * m_resolution.z);
	else if (outRowPitch == inRowPitch)
	{
		for (int z = 0; z < m_resolution.z; ++z)
			std::memcpy(outData + z * outDepthPitch, inData + z * inDepthPitch, inDepthPitch);
	}
	else
	{
		for (int z = 0; z < m_resolution.z; ++z)
		{
			for (int y = 0; y < m_resolution.y; ++y)
				std::memcpy(outData + y * outRowPitch + z * outDepthPitch, inData + y * inRowPitch + z * inDepthPitch, inRowPitch);
		}
	}
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
	D3D11_VIEWPORT vpSolid; // Used for the solid voxelization
	vpSolid.TopLeftX = 0.0f;
	vpSolid.TopLeftY = 0.0f;
	vpSolid.Width = static_cast<float>(m_resolution.z * factor);
	vpSolid.Height = static_cast<float>(m_resolution.y * factor);
	vpSolid.MinDepth = 0.0f;
	vpSolid.MaxDepth = 1.0f;

	factor = 32.0f; // The higher, the more conservative
	D3D11_VIEWPORT vpCons; // Used for conservative voxelization
	vpCons.TopLeftX = 0.0f;
	vpCons.TopLeftY = 0.0f;
	vpCons.Width = static_cast<float>(m_resolution.z * factor);
	vpCons.Height = static_cast<float>(m_resolution.y * factor);
	vpCons.MinDepth = 0.0f;
	vpCons.MaxDepth = 1.0f;


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

	// Decompose voxel grid world matrix
	XMVECTOR trans;
	XMVECTOR rot;
	XMVECTOR scale;
	XMMatrixDecompose(&scale, &rot, &trans, XMLoadFloat4x4(&world));

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
			XMMATRIX objToWorld = XMLoadFloat4x4(&ma->getDynWorld());
			s_shaderVariables.objToWorld->SetMatrix(reinterpret_cast<float*>(objToWorld.r));

			s_shaderVariables.gridUAV->SetUnorderedAccessView(m_gridUAV);
			s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

			context->RSSetViewports(1, &vpSolid);
			ID3D11Buffer* vb = ma->getMesh().getVertexBuffer();
			ID3D11Buffer* ib = ma->getMesh().getIndexBuffer();
			context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
			context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
			context->DrawIndexed(ma->getMesh().getNumIndices(), 0, 0);

			s_shaderVariables.gridUAV->SetUnorderedAccessView(nullptr);
			s_effect->GetTechniqueByIndex(0)->GetPassByName("Voxelize")->Apply(0, context);

			if (m_conservative)
			{
				s_shaderVariables.gridUAV->SetUnorderedAccessView(m_gridUAV);
				s_effect->GetTechniqueByIndex(0)->GetPassByName("Conservative")->Apply(0, context);

				context->RSSetViewports(1, &vpCons);
				context->DrawIndexed(ma->getMesh().getNumIndices(), 0, 0);

				s_shaderVariables.gridUAV->SetUnorderedAccessView(nullptr);
				s_effect->GetTechniqueByIndex(0)->GetPassByName("Conservative")->Apply(0, context);
			}

			// ###################################
			// COMBINE
			// ###################################
			// Use compute shader to combine this meshes voxelization with the former ones
			// Also calculate the linear velocity for each solid voxel
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
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellTypeGridBoundary")->Apply(0, context);

	context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

	s_shaderVariables.gridAllUAV->SetUnorderedAccessView(nullptr);
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellTypeGridBoundary")->Apply(0, context);

	s_shaderVariables.gridAllUAV->SetUnorderedAccessView(m_gridAllUAV);
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellTypeSolidBoundary")->Apply(0, context);

	context->Dispatch(dispatch.x, dispatch.y, dispatch.z);

	s_shaderVariables.gridAllUAV->SetUnorderedAccessView(nullptr);
	s_effect->GetTechniqueByIndex(0)->GetPassByName("CellTypeSolidBoundary")->Apply(0, context);

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

	std::string passName = "SolidVoxel";
	if (m_voxelType == Wireframe)
		passName = "WireframeVoxel";

	XMMATRIX voxelToGrid = XMMatrixScalingFromVector(XMLoadFloat3(&m_voxelSize));
	XMMATRIX voxelWorldView = voxelToGrid * w * v;
	XMMATRIX voxelWorldViewProj = voxelWorldView * p;
	s_shaderVariables.voxelWorldViewProj->SetMatrix(reinterpret_cast<float*>(voxelWorldViewProj.r));
	s_shaderVariables.voxelWorldView->SetMatrix(reinterpret_cast<float*>(voxelWorldView.r));
	s_shaderVariables.resolution->SetIntVector(reinterpret_cast<const int*>(&m_resolution));
	s_shaderVariables.gridAllSRV->SetResource(m_gridAllSRV);

	s_effect->GetTechniqueByIndex(0)->GetPassByName(passName.c_str())->Apply(0, context);

	UINT stride = 0;
	UINT offset = 0;
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->IASetVertexBuffers(0, 0, nullptr, &stride, &offset);
	context->Draw(m_resolution.x * m_resolution.y * m_resolution.z, 0);

	s_shaderVariables.gridAllSRV->SetResource(nullptr);
	s_effect->GetTechniqueByIndex(0)->GetPassByName(passName.c_str())->Apply(0, context);
}

void VoxelGrid::renderGlyphs(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
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

	s_shaderVariables.glyphOrientation->SetInt(m_glyphOrientation);
	s_shaderVariables.glyphPosition->SetFloat(m_glyphPosition);
	s_shaderVariables.glyphQuantity->SetIntVector(reinterpret_cast<int*>(&m_glyphQuantity));

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
			ma->calculateDynamics(device, context, worldToVoxelTex, m_resolution, m_voxelSize, conf.dyn.method == Pressure ? m_pressureSRV : m_velocitySRV, elapsedTime);
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