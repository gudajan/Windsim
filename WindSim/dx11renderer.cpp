#include "dx11renderer.h"
#include "mesh.h"

#include <iostream>
#include <cassert>
#include <sstream>

#include <DirectXMath.h>
#include <DirectXColors.h>

#include <QCoreApplication>
#include <QDir>

using namespace DirectX;


DX11Renderer::DX11Renderer(WId hwnd, int width, int height)
	: m_windowHandle(hwnd),
	m_device(nullptr),
	m_context(nullptr),
	m_swapChain(nullptr),
	m_depthStencilBuffer(nullptr),
	m_renderTargetView(nullptr),
	m_depthStencilView(nullptr),
	m_rasterizerState(nullptr),
	m_width(width),
	m_height(height),
	m_stopped(false),
	m_elapsedTimer(),
	m_renderTimer(this),
	m_camera(width, height, FirstPerson),
	m_manager()
{
	//m_camera.setType(ModelView);

	connect(&m_renderTimer, &QTimer::timeout, this, &DX11Renderer::frame);
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

	D3D_FEATURE_LEVEL featureLevel;

	if (FAILED(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, 0, 0, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context)))
		return false;

	if (featureLevel != D3D_FEATURE_LEVEL_11_0)
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
		FALSE,//BOOL MultisampleEnable;
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

	OutputDebugStringA("Initialized DirectX 11\n");

	// Create all shaders
	QString fxoPath = QDir(QCoreApplication::applicationDirPath()).filePath("mesh.fxo");
	if (FAILED(Mesh::createShaderFromFile(fxoPath.toStdWString() , m_device)))
		return false;

	return true;
}


void DX11Renderer::frame()
{
	double elapsedTime = m_elapsedTimer.restart() * 0.001;
	update(elapsedTime);
	render(elapsedTime);
}

void DX11Renderer::update(double elapsedTime)
{
	m_camera.update(elapsedTime);
}

void DX11Renderer::render(double elapsedTime)
{
	m_context->ClearRenderTargetView(m_renderTargetView, DirectX::Colors::Azure);
	m_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_manager.render(m_device, m_context, m_camera.getViewMatrix(), m_camera.getProjectionMatrix());

	m_swapChain->Present(0, 0);
}


void DX11Renderer::execute()
{
	m_elapsedTimer.start();
	m_renderTimer.start(1000.0f / 120.0f); // Rendering happens with 120 FPS at max

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
	Mesh::releaseShader();

	// Release all objects
	m_manager.release();

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

void DX11Renderer::stop()
{
	m_renderTimer.stop();
	destroy();
}

void DX11Renderer::onControlEvent(QEvent* event)
{
	m_camera.handleControlEvent(event);
}

void DX11Renderer::onMeshCreated(const QString& name, const QString& path)
{
	m_manager.add(name.toStdString(), m_device, path.toStdString(), ObjectType::Mesh);
}

