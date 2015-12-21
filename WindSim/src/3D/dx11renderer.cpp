#include "dx11renderer.h"
#include "mesh3D.h"
#include "sky.h"
#include "axes.h"
#include "marker.h"
#include "voxelGrid.h"
#include "settings.h"

#include <iostream>
#include <cassert>
#include <sstream>

// DirectX
#include <DirectXColors.h>
#include <d3d11.h>
#include <d3dx11effect.h>

#include <QCoreApplication>
#include <QDir>
#include <QThread>

using namespace DirectX;

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
	m_width(width),
	m_height(height),
	m_containsCursor(false),
	m_localCursorPos(),
	m_pressedId(0),
	m_state(State::Default),
	m_elapsedTimer(),
	m_renderTimer(this),
	m_voxelizationTimer(this),
	m_camera(width, height),
	m_manager(),
	m_transformer(&m_manager, &m_camera)
{
	connect(&m_renderTimer, &QTimer::timeout, this, &DX11Renderer::frame);
	connect(&m_voxelizationTimer, &QTimer::timeout, this, &DX11Renderer::issueVoxelization);
}

DX11Renderer::~DX11Renderer()
{

}

bool DX11Renderer::init()
{
	unsigned int createDeviceFlags = 0;

#ifndef NDEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	const D3D_FEATURE_LEVEL required[] = { D3D_FEATURE_LEVEL_11_1 };
	D3D_FEATURE_LEVEL featureLevel;

	if (FAILED(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, required, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context)))
		return false;

	if (featureLevel != D3D_FEATURE_LEVEL_11_1)
		return false;

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
	if (FAILED(m_device->CreateRasterizerState(&drd, &m_rasterizerState)))
		return false;

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

	if (FAILED(dxgiFactory->CreateSwapChain(m_device, &scDesc, &m_swapChain)))
		return false;

	SAFE_RELEASE(dxgiDevice);
	SAFE_RELEASE(dxgiAdapter);
	SAFE_RELEASE(dxgiFactory);

	// Initialize rendertargets and set viewport
	onResize(m_width, m_height);

	m_transformer.initDX11(m_device);

	OutputDebugStringA("Initialized DirectX 11\n");

	return createShaders();
}



void DX11Renderer::frame()
{
	double elapsedTime = m_elapsedTimer.restart() * 0.001; // Milliseconds to seconds
	update(elapsedTime);
	render(elapsedTime);
	emit updateFPS(static_cast<int>(1.0 / elapsedTime)); // Show fps in statusBar
}

void DX11Renderer::issueVoxelization()
{
	m_manager.voxelizeNextFrame();
}

void DX11Renderer::execute()
{
	OutputDebugStringA(("WORKER THREAD: " + std::to_string(reinterpret_cast<int>(thread()->currentThreadId())) + "\n").c_str());
	m_elapsedTimer.start();
	m_renderTimer.start(1000.0f / 120.0f); // Rendering happens with 120 FPS at max
	m_voxelizationTimer.start(1000.0f / 40.0f); // Voxelization happens 40 times per second at maximum
}

void DX11Renderer::stop()
{
	m_renderTimer.stop();
	m_voxelizationTimer.stop();
	destroy();
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
	m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
	SAFE_RELEASE(pBackBuffer);

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

	if (button == Qt::LeftButton)
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

void DX11Renderer::onSelectionChanged(std::unordered_set<int> ids)
{
	m_manager.setSelection(ids); // Set selected ids
	m_manager.setSelected(); // Set selection states of objects
}

bool DX11Renderer::reloadShaders()
{
	if (FAILED(Mesh3D::createShaderFromFile(L"src\\3D\\shaders\\mesh.fx", m_device, true)))
		return false;

	if (FAILED(Sky::createShaderFromFile(L"src\\3D\\shaders\\sky.fx", m_device, true)))
		return false;

	if (FAILED(Axes::createShaderFromFile(L"src\\3D\\shaders\\axes.fx", m_device, true)))
		return false;

	if (FAILED(Marker::createShaderFromFile(L"src\\3D\\shaders\\marker.fx", m_device, true)))
		return false;

	if (FAILED(VoxelGrid::createShaderFromFile(L"src\\3D\\shaders\\voxelGrid.fx", m_device, true)))
		return false;

	return true;
}

void DX11Renderer::reloadIni()
{
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

	// Release all objects
	m_manager.release(true);

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
	m_camera.update(elapsedTime);

	// Pass ray from Camera to cursor position in world space
	m_manager.updateCursor(m_camera.getCamPos(), m_camera.getCursorDir(m_localCursorPos), m_containsCursor);
	m_manager.setHovered();
}

void DX11Renderer::render(double elapsedTime)
{
	m_context->ClearRenderTargetView(m_renderTargetView, Colors::WhiteSmoke);
	m_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH , 1.0f, 0);

	m_manager.render(m_device, m_context, m_camera.getViewMatrix(), m_camera.getProjectionMatrix());

	m_swapChain->Present(0, 0);
}
