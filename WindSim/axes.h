#ifndef AXES_H
#define AXES_H

#include "object3D.h"

struct ID3DX11EffectMatrixVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;

// A class for rendering the 3 coordinate axes as lines into the 3D view
class Axes : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Axes();
	Axes(Axes&& other);
	~Axes();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

private:
	void createAxesData();

	static ID3DX11EffectMatrixVariable* s_worldViewProj;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_inputLayout;

};
#endif