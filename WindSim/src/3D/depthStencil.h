#ifndef DEPTH_STENCIL_H
#define DEPTH_STENCIL_H

#include <d3d11.h>

class DepthStencil
{
public:
	DepthStencil() = delete;
	~DepthStencil() = delete;

	static HRESULT create(ID3D11Device* device, UINT width, UINT height);

	static void release();

	static void update(ID3D11DeviceContext* context);

	static ID3D11ShaderResourceView* srv() { return _srv; };

private:
	static ID3D11ShaderResourceView* _srv;
	static ID3D11Texture2D* _tex;

};
#endif