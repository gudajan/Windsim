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

	Axes(DX11Renderer* renderer);

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;

private:
	void createAxesData();

	struct ShaderVariables
	{
		ID3DX11EffectMatrixVariable* view;
		ID3DX11EffectMatrixVariable* proj;
		ID3DX11EffectMatrixVariable* viewProj;
		ID3DX11EffectMatrixVariable* viewProjInv;
		ID3DX11EffectMatrixVariable* worldViewProj;
	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_inputLayout;

};
#endif