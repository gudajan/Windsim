#ifndef MARKER_H
#define MARKER_H

#include "object3D.h"

struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectScalarVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;

class Marker : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Marker();
	Marker(Marker&& other);
	~Marker();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

	void setPositionRendered(bool render) { m_renderPosition = render; };
	void setXAxisRendered(bool render) { m_renderX = render; };
	void setYAxisRendered(bool render) { m_renderY = render; };
	void setZAxisRendered(bool render) { m_renderZ = render; };
	void setLarge(bool large) { m_renderLarge = large; };

private:
	// Indicate which parts of the markers are currently rendered
	bool m_renderPosition;
	bool m_renderX;
	bool m_renderY;
	bool m_renderZ;

	bool m_renderLarge; // Indicates if the axes are drawn until infinity

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