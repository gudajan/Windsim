#ifndef MESH_H
#define MESH_H

#include "object3D.h"

struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectScalarVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;

class Mesh : public Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Mesh(const std::string& path);
	Mesh(Mesh&& other);
	virtual ~Mesh();

	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);

private:
	bool readObj(const std::string& path);

	struct ShaderVariables
	{
		ShaderVariables();
		virtual ~ShaderVariables();

		ID3DX11EffectMatrixVariable* worldView;
		ID3DX11EffectMatrixVariable* worldViewIT; // Inverse-Transposed World View
		ID3DX11EffectMatrixVariable* worldViewProj;
		ID3DX11EffectScalarVariable* enableFlatShading;
	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_inputLayout;
};
#endif