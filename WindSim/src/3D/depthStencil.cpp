#include "depthStencil.h"
#include "common.h"

#include <iostream>

ID3D11ShaderResourceView* DepthStencil::_srv = nullptr;
ID3D11Texture2D* DepthStencil::_tex = nullptr;

HRESULT DepthStencil::create(ID3D11Device* device, UINT width, UINT height)
{
	SAFE_RELEASE(_srv);
	SAFE_RELEASE(_tex);

	HRESULT hr;

	D3D11_TEXTURE2D_DESC td;
	td.Width = width;
	td.Height = height;
	td.Format = DXGI_FORMAT_R24G8_TYPELESS;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.ArraySize = 1;
	td.CPUAccessFlags = 0;
	td.MipLevels = 1;
	td.MiscFlags = 0;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DEFAULT;

	hr = device->CreateTexture2D(&td, 0, &_tex);
	if (FAILED(hr))
	{
		OutputDebugStringA("FAILED to create DepthStencil texture\n");
		std::cerr << "ERROR: Failed to create DepthStencil texture.\n";
		return hr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
	srvd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	srvd.Texture2D.MostDetailedMip = 0;

	hr = device->CreateShaderResourceView(_tex, &srvd, &_srv);
	if (FAILED(hr))
	{
		OutputDebugStringA("FAILED to create DepthStencil SRV\n");
		std::cerr << "ERROR: Failed to create DepthStencil SRV.\n";
		return hr;
	}

	return S_OK;
}

void DepthStencil::release()
{
	SAFE_RELEASE(_tex);
	SAFE_RELEASE(_srv);
}

void DepthStencil::update(ID3D11DeviceContext* context)
{
	ID3D11Resource* dsr;
	ID3D11DepthStencilView* dsv = nullptr;
	context->OMGetRenderTargets(1, nullptr, &dsv);
	dsv->GetResource(&dsr);
	context->CopyResource(_tex, dsr);
	SAFE_RELEASE(dsr);
	SAFE_RELEASE(dsv);
}