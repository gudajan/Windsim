#include "dx11renderer.h"

#include <iostream>
#include <cassert>
#include <sstream>

#include <QDebug>

#include <DirectXMath.h>
#include <DirectXColors.h>

static bool color = true;

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
	m_height(height)
{
}

DX11Renderer::~DX11Renderer()
{
}

bool DX11Renderer::onInit()
{
	unsigned int createDeviceFlags = 0;

#ifndef NDEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, 0, 0, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);

	if (FAILED(hr)) return false;

	if (featureLevel != D3D_FEATURE_LEVEL_11_0) return false;

	IDXGIDevice* dxgiDevice = nullptr;
	m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<LPVOID*>(&dxgiDevice));

	IDXGIAdapter* dxgiAdapter = nullptr;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<LPVOID*>(&dxgiAdapter));

	IDXGIFactory* dxgiFactory = nullptr;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<LPVOID*>(&dxgiFactory));

	// set default render state to msaa enabled
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
	hr = m_device->CreateRasterizerState(&drd, &m_rasterizerState);
	
	if (FAILED(hr)) return false;

	m_context->RSSetState(m_rasterizerState);

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
	scDesc.BufferCount = 1;
	scDesc.OutputWindow = reinterpret_cast<HWND>(m_windowHandle);
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	hr = dxgiFactory->CreateSwapChain(m_device, &scDesc, &m_swapChain);

	if (FAILED(hr)) return false;

	SAFE_RELEASE(dxgiDevice);
	SAFE_RELEASE(dxgiAdapter);
	SAFE_RELEASE(dxgiFactory);

	onResize(m_width, m_height);

	return true;
}

void DX11Renderer::onUpdate()
{

}

void DX11Renderer::onRender()
{
	if (color) m_context->ClearRenderTargetView(m_renderTargetView, DirectX::Colors::Azure);
	else m_context->ClearRenderTargetView(m_renderTargetView, DirectX::Colors::Chocolate);
	color = !color;

	m_swapChain->Present(0, 0);

	qDebug() << "TEST RENDER";
}

void DX11Renderer::onResize(int width, int height)
{
	m_width = width;
	m_height = height;

	assert(m_context);
	assert(m_device);
	assert(m_swapChain);

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

	// Setup the viewport to match the backbuffer
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(m_width);
	vp.Height = static_cast<float>(m_height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	m_context->RSSetViewports(1, &vp);

	// Bind to pipeline
	m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

}

void DX11Renderer::onDestroy()
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
			std::stringstream s;
			s << "ERROR: " << references << " UNRELEASED REFERENCES!\n";
			OutputDebugStringA(s.str().c_str());
		}
	}
}