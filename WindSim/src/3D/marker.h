#ifndef MARKER_H
#define MARKER_H

#include "object3D.h"

struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectScalarVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;
class Logger;

class Marker : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Marker(Logger* logger);
	Marker(Marker&& other);
	~Marker();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	void setPositionRendered(bool render) { m_renderPosition = render; };

	void setShaderVariables(bool renderX, bool renderY, bool renderZ, bool renderLarge);

private:
	bool m_renderPosition; // Indicates if position is rendered
	struct ShaderVariables
	{
		ShaderVariables();
		virtual ~ShaderVariables();

		ID3DX11EffectMatrixVariable* worldView;
		ID3DX11EffectMatrixVariable* projection;
		ID3DX11EffectMatrixVariable* worldViewProj;
		ID3DX11EffectScalarVariable* renderX;
		ID3DX11EffectScalarVariable* renderY;
		ID3DX11EffectScalarVariable* renderZ;
		ID3DX11EffectScalarVariable* renderLarge;
	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_inputLayout;

};
#endif