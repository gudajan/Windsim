#include "dx11renderer.h"
#include "mesh3D.h"
#include "sky.h"
#include "axes.h"
#include "marker.h"
#include "voxelGrid.h"
#include "voxelGridActor.h"
#include "simulator.h"
#include "dynamics.h"
#include "settings.h"


#include <iostream>
#include <cassert>
#include <sstream>
#include <algorithm>

// DirectX
#include <DirectXColors.h>
#include <d3dx11effect.h>

#include <comdef.h>

#include <QCoreApplication>
#include <QDir>
#include <QThread>

using namespace DirectX;

const int fpsFramesSaved = 20;
const float fpsFramesWeight = 1.0 / fpsFramesSaved;

DX11Renderer::DX11Renderer(WId hwnd, int width, int height, QObject* parent)
	: QObject(parent),
	m_windowHandle(hwnd),
	m_device(nullptr),
	m_context(nullptr),
	m_swapChain(nullptr),
	m_depthStencilBuffer(nullptr),
	m_renderTargetView(nullptr),
	m_depthStencilView(nullptr),
	m_rasterizerState(nullptr),
	m_backBufferDesc(),
	m_width(width),
	m_height(height),
	m_containsCursor(false),
	m_localCursorPos(),
	m_pressedId(0),
	m_state(State::Default),
	m_renderingPaused(true),
	m_elapsedTimes(fpsFramesSaved, 0),
	m_currentFPS(0),
	m_elapsedTimer(),
	m_renderTimer(this),
	m_voxelizationTimer(this),
	m_camera(width, height),
	m_manager(this),
	m_transformer(&m_manager, &m_camera, this),
	m_logger()
{
	connect(&m_renderTimer, &QTimer::timeout, this, &DX11Renderer::frame);
	connect(&m_voxelizationTimer, &QTimer::timeout, this, &DX11Renderer::issueVoxelization);
}

DX11Renderer::~DX11Renderer()
{
	//OutputDebugStringA("Destr DX11Renderer\n");
}

bool DX11Renderer::init()
{
	if (Simulator::initOpenCLNecessary())
		Simulator::initOpenCL();

	unsigned int createDeviceFlags = 0;

#ifndef NDEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	const D3D_FEATURE_LEVEL required[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL featureLevel;

	HRESULT result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, required, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
	if (FAILED(result))
	{
		_com_error err(result);
		std::cout << "ERROR: Failed to create directX device with message '" << err.ErrorMessage() << "'\n";
		return false;
	}

	if (featureLevel != D3D_FEATURE_LEVEL_11_0)
	{
		std::cout << "ERROR: Failed to create directX device. Feature level is different to DirectX 11.0.\n";
		return false;
	}

	// Create rasterizerstate
	// set default render state to msaa disabled
	D3D11_RASTERIZER_DESC drd = {
		D3D11_FILL_SOLID, //D3D11_FILL_MODE FillMode;
		D3D11_CULL_BACK,//D3D11_CULL_MODE CullMode;
		FALSE, //BOOL FrontCounterClockwise;
		0, //INT DepthBias;
		0.0f,//FLOAT DepthBiasClamp;
		0.0f,//FLOAT SlopeScaledDepthBias;
		TRUE,//BOOL DepthClipEnable;
		FALSE,//BOOL ScissorEnable;
		TRUE,//BOOL MultisampleEnable;
		FALSE//BOOL AntialiasedLineEnable;
	};
	result = m_device->CreateRasterizerState(&drd, &m_rasterizerState);
	if (FAILED(result))
	{
		_com_error err(result);
		std::cout << "ERROR: Failed to create rasterizer state with message '" << err.ErrorMessage() << "'\n";
		return false;
	}

	m_context->RSSetState(m_rasterizerState);

	// Create swapchain
	DXGI_SWAP_CHAIN_DESC scDesc;
	scDesc.BufferDesc.Width = m_width;
	scDesc.BufferDesc.Height = m_height;
	scDesc.BufferDesc.RefreshRate.Numerator = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = 32;
	scDesc.BufferCount = 2;
	scDesc.OutputWindow = reinterpret_cast<HWND>(m_windowHandle);
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	IDXGIDevice* dxgiDevice = nullptr;
	m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<LPVOID*>(&dxgiDevice));

	IDXGIAdapter* dxgiAdapter = nullptr;
	dxgiDevice->GetAdapter(&dxgiAdapter);

	IDXGIFactory* dxgiFactory = nullptr;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<LPVOID*>(&dxgiFactory));

	result = dxgiFactory->CreateSwapChain(m_device, &scDesc, &m_swapChain);
	if (FAILED(result))
	{
		_com_error err(result);
		std::cout << "ERROR: Failed to create directX swapchain with message '" << err.ErrorMessage() << "'\n";
		return false;
	}

	SAFE_RELEASE(dxgiDevice);
	SAFE_RELEASE(dxgiAdapter);
	SAFE_RELEASE(dxgiFactory);

	// Initialize rendertargets and set viewport
	onResize(m_width, m_height);

	m_transformer.initDX11(m_device);

	OutputDebugStringA("Initialized DirectX 11\n");

	return createShaders();
}

void DX11Renderer::drawInfo(const QString& info)
{
	emit drawText(info);
}

void DX11Renderer::frame()
{
	if (m_state == State::ShaderError)
		return;

	long long elapsedTime = m_elapsedTimer.nsecsElapsed(); // nanoseconds
	//OutputDebugStringA(("Current FPS: " + std::to_string(m_currentFPS) + "\n").c_str());
	//OutputDebugStringA(("Elapsed: " + std::to_string(elapsedTime * 0.000001) + "msec\n").c_str());
	m_elapsedTimer.restart();
	double t = static_cast<double>(elapsedTime) * 1.0e-9; // to seconds
	update(t);
	render(t);

	m_elapsedTimes.pop_back();
	m_elapsedTimes.push_front(1.0e9 / elapsedTime); // 1 sec / elapsedTime nsec = fps
	m_currentFPS = std::accumulate(m_elapsedTimes.begin(), m_elapsedTimes.end(), 0.0, [](float acc, const float& val) {return acc + fpsFramesWeight * val; });
	emit updateFPS(m_currentFPS); // Show fps
}

void DX11Renderer::issueVoxelization()
{
	m_manager.voxelizeNextFrame();
}

void DX11Renderer::execute()
{
	m_elapsedTimer.start();
	m_renderTimer.start(8); // Rendering happens with 125 FPS at max
	m_voxelizationTimer.start(125); // Voxelization happens 40 times per second at maximum
}

void DX11Renderer::stop()
{
	m_renderTimer.stop();
	m_voxelizationTimer.stop();
	destroy();
	thread()->quit(); // Quit the thread
}

void DX11Renderer::cont()
{
	m_renderingPaused = false;
}

void DX11Renderer::pause()
{
	m_renderingPaused = true;
}

void DX11Renderer::onResize(int width, int height)
{
	m_width = width;
	m_height = height;

	m_camera.setAspectRatio(width, height);

	assert(m_context);
	assert(m_device);
	assert(m_swapChain);

	m_context->OMSetRenderTargets(0, 0, 0);

	SAFE_RELEASE(m_renderTargetView);
	SAFE_RELEASE(m_depthStencilView);
	SAFE_RELEASE(m_depthStencilBuffer);

	m_swapChain->ResizeBuffers(1, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	ID3D11Texture2D* pBackBuffer = nullptr;
	m_swapChain->GetBuffer(0, __uuidof(*pBackBuffer), reinterpret_cast<LPVOID*>(&pBackBuffer));
	IDXGISurface* pSurface = nullptr;
	pBackBuffer->QueryInterface(__uuidof(pSurface), reinterpret_cast<LPVOID*>(&pSurface));
	pSurface->GetDesc(&m_backBufferDesc);
	m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
	SAFE_RELEASE(pBackBuffer);
	SAFE_RELEASE(pSurface);

	D3D11_TEXTURE2D_DESC descDepth;
	descDepth.Width = m_width;
	descDepth.Height = m_height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	m_device->CreateTexture2D(&descDepth, 0, &m_depthStencilBuffer);

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = descDepth.Format;
	descDSV.Flags = 0;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	m_device->CreateDepthStencilView(m_depthStencilBuffer, &descDSV, &m_depthStencilView);

	// Bind to pipeline
	m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

	// Setup the viewport to match the backbuffer
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(m_width);
	vp.Height = static_cast<float>(m_height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	m_context->RSSetViewports(1, &vp);

	m_manager.onResizeSwapChain(m_device, &m_backBufferDesc);
}

void DX11Renderer::onMouseMove(QPoint localPos, QPoint globalPos, int modifiers)
{
	m_localCursorPos = localPos;
	Qt::KeyboardModifiers m = Qt::KeyboardModifiers(modifiers);

	switch (m_state)
	{
	case(State::Default) :
		break;
	case(State::CameraMove):
		m_camera.handleMouseMove(globalPos);
		break;
	case(State::Modifying) :
		m_transformer.handleMouseMove(localPos, m);
		break;
	}

}

void DX11Renderer::onMousePress(QPoint globalPos, int button, int modifiers)
{
	Qt::MouseButton b = Qt::MouseButton(button);
	Qt::KeyboardModifiers m = Qt::KeyboardModifiers(modifiers);

	bool wasModifying = false;

	switch (m_state)
	{
	case(State::Default):
		m_camera.handleMousePress(globalPos, b, m);
		if (m_camera.isMoving())
			m_state = State::CameraMove; // Default -> CameraMove;
		break;
	case(State::CameraMove) :
		m_camera.handleMousePress(globalPos, b, m);
		break;
	case(State::Modifying) :
		wasModifying = true;
		// (righclick -> abort, leftclick -> ok -> create modify cmd
		m_transformer.handleMousePress(b);
		if (!m_transformer.isModifying())
		{
			m_state = State::Default; // Modifying -> Default;
			if (!m_transformer.isAborted())
				emit modify(m_transformer.getTransformation());
			m_transformer.reset();
		}
		break;
	}

	if (!wasModifying && button == Qt::LeftButton)
		m_pressedId = m_manager.getHoveredId();
}

void DX11Renderer::onMouseRelease(QPoint globalPos, int button, int modifiers)
{
	Qt::MouseButton b = Qt::MouseButton(button);
	Qt::KeyboardModifiers m = Qt::KeyboardModifiers(modifiers);
	switch (m_state)
	{
	case(State::Default) :
		break;
	case(State::CameraMove) :
		m_camera.handleMouseRelease(globalPos, b);
		if (!m_camera.isMoving())
		{
			m_state = State::Default; // CameraMove -> Default
		}

		break;
	case(State::Modifying) : // State is left on mouse press -> nothing to do
		break;
	}

	// This is in the CameraMove State, as we think of a very small camera movement, while hovering over an object
	// to select an object -> The camera must have been moving during mouse press and release
	if (m_pressedId == m_manager.getHoveredId()) // Hovered id did not change since mouse press -> change selection
	{
		if (b == Qt::LeftButton)
		{
			bool changed = false;
			if (m == Qt::NoModifier)
				changed = m_manager.updateSelection(Selection::Replace);
			else if (m == Qt::ControlModifier)
				changed = m_manager.updateSelection(Selection::Switch);

			if (changed)
				emit selectionChanged(m_manager.getSelection());
		}
	}
}

void DX11Renderer::onKeyPress(int key)
{
	Qt::Key k = Qt::Key(key);

	switch (m_state)
	{
	case(State::Default) :
		m_camera.handleKeyPress(k);
		if (m_camera.isMoving())
		{
			m_state = State::CameraMove; // Default -> CameraMove
			break;
		}

		m_transformer.handleKeyPress(k, m_localCursorPos);
		if (m_transformer.isModifying())
			m_state = State::Modifying; // Default -> Modifying
		break;
	case(State::CameraMove) :
		m_camera.handleKeyPress(k);
		break;
	case(State::Modifying) :
		m_transformer.handleKeyPress(k, m_localCursorPos);
		if (!m_transformer.isModifying())
		{
			m_state = State::Default; // Modifying -> Default;
			if (!m_transformer.isAborted())
				emit modify(m_transformer.getTransformation());
			m_transformer.reset();
		}
		break;
	}
}

void DX11Renderer::onKeyRelease(int key)
{
	Qt::Key k = Qt::Key(key);

	switch (m_state)
	{
	case(State::Default) :
		break;
	case(State::CameraMove) :
		m_camera.handleKeyRelease(k);
		if (!m_camera.isMoving())
			m_state = State::Default; // CameraMove -> Default
		break;
	case(State::Modifying) : // States within transform machine are changed on key press -> nothing to do
		break;
	}
}

void DX11Renderer::onWheelUse(QPoint angleDelta)
{
	if (m_state != State::Modifying)
		m_camera.handleWheel(angleDelta);
}
void DX11Renderer::onAddObject(const QJsonObject& data)
{
	m_manager.add(m_device, data);
}

void DX11Renderer::onModifyObject(const QJsonObject& data)
{
	m_manager.modify(data);
}

void DX11Renderer::onRemoveObject(int id)
{
	m_manager.remove(id);
}

void DX11Renderer::onRemoveAll()
{
	m_manager.removeAll();
}

void DX11Renderer::onTriggerFunction(const QJsonObject& data)
{
	m_manager.triggerObjectFunction(data);
}

void DX11Renderer::onSelectionChanged(std::unordered_set<int> ids)
{
	m_manager.setSelection(ids); // Set selected ids
	m_manager.setSelected(); // Set selection states of objects
}

bool DX11Renderer::reloadShaders()
{
	bool reloaded = true;
	if (FAILED(Mesh3D::createShaderFromFile(L"src\\3D\\shaders\\mesh.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\mesh.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (FAILED(Sky::createShaderFromFile(L"src\\3D\\shaders\\sky.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\sky.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (FAILED(Axes::createShaderFromFile(L"src\\3D\\shaders\\axes.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\axes.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (FAILED(Marker::createShaderFromFile(L"src\\3D\\shaders\\marker.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\marker.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (FAILED(VoxelGrid::createShaderFromFile(L"src\\3D\\shaders\\voxelGrid.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\voxelGrid.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (FAILED(Dynamics::createShaderFromFile(L"src\\3D\\shaders\\dynamics.fx", m_device, true)))
	{
		m_logger.logit("WARNING: Failed to reload shader file 'src\\3D\\shaders\\dynamics.fx'! Stopped rendering.");
		m_state = State::ShaderError;
		reloaded = false;
	}

	if (reloaded)
	{
		m_logger.logit("INFO: Shaders successfully reloaded.");
		m_state = State::Default;
	}

	return reloaded;
}

void DX11Renderer::changeSettings()
{
	if (Simulator::initOpenCLNecessary())
	{
		m_logger.logit("INFO: Reinitializing OpenCL...");
		QMutexLocker lock(&Simulator::mutex()); // Lock all simulations (just avoids new simulation step events to be generated)
		m_manager.runSimulation(false); // Blocks until all currently running simulation steps finished
		Simulator::initOpenCL();
		m_logger.logit("INFO: Done.");
		// Init all objects that require OpenCL to use new OpenCL objects
		// within THIS thread so we can synchronize with the static operation
		m_manager.initOpenCL();
		m_manager.runSimulation(true); // Continue simulation again
	}
	m_camera.computeViewMatrix(); // Depends on camera type
}

bool DX11Renderer::createShaders()
{
	QString fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("mesh.fxo");
	if (FAILED(Mesh3D::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("sky.fxo");
	if (FAILED(Sky::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("axes.fxo");
	if (FAILED(Axes::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("marker.fxo");
	if (FAILED(Marker::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("voxelGrid.fxo");
	if (FAILED(VoxelGrid::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("dynamics.fxo");
	if (FAILED(Dynamics::createShaderFromFile(fxoPath.toStdWString(), m_device)))
		return false;

	return true;
}

void DX11Renderer::destroy()
{
	SAFE_RELEASE(m_depthStencilBuffer);
	SAFE_RELEASE(m_depthStencilView);
	SAFE_RELEASE(m_renderTargetView);
	SAFE_RELEASE(m_rasterizerState);

	if (m_swapChain) m_swapChain->SetFullscreenState(FALSE, nullptr);
	SAFE_RELEASE(m_swapChain);

	if (m_context)
	{
		m_context->ClearState();
		m_context->Flush();
	}
	SAFE_RELEASE(m_context);

	// Release all shaders
	Mesh3D::releaseShader();
	Sky::releaseShader();
	Axes::releaseShader();
	Marker::releaseShader();
	VoxelGrid::releaseShader();
	Dynamics::releaseShader();

	// Release all objects
	m_manager.removeAll();
	m_manager.release(true);

	Simulator::shutdownOpenCL();

	if (m_device)
	{
#ifndef NDEBUG
		ID3D11Debug * debug = nullptr;
		if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<LPVOID*>(&debug))))
		{
			debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
			debug->Release();
		}
#endif
		UINT references = m_device->Release();
		if (references > 0)
		{
			// TODO: MAYBE ERROR WINDOW
			OutputDebugStringA(std::string("ERROR: " + std::to_string(references) + " UNRELEASED REFERENCES!\n").c_str());
		}
	}
}

void DX11Renderer::update(double elapsedTime)
{
	if (m_renderingPaused) return;

	m_camera.update(elapsedTime);

	// Pass ray from Camera to cursor position in world space
	m_manager.updateCursor(m_camera.getCamPos(), m_camera.getCursorDir(m_localCursorPos), m_containsCursor);
	m_manager.setHovered();
}

void DX11Renderer::render(double elapsedTime)
{
	FLOAT color[] = { 240.0f / 255.0f, 240.0f / 255.0f, 240.0f / 255.0f, 1.0f }; // Qt pale gray
	m_context->ClearRenderTargetView(m_renderTargetView, color);
	m_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH , 1.0f, 0);

	if (!m_renderingPaused)
		m_manager.render(m_device, m_context, m_camera.getViewMatrix(), m_camera.getProjectionMatrix(), elapsedTime);

	m_swapChain->Present(0, 0);
}
