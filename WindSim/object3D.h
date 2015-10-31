#ifndef OBJECT_3D_H
#define OBJECT_3D_H

#include <DirectXMath.h>
#include <Windows.h>

#include <vector>

#include "common.h"

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3DX11EffectMatrixVariable;
struct ID3DX11Effect;
struct ID3D11InputLayout;

class Object3D
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	Object3D(const std::string& path);
	Object3D(Object3D&& other);
	virtual ~Object3D();

	virtual HRESULT create(ID3D11Device* device, bool clearClientBuffers = true);
	virtual void release();

	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world);

	//void Object3D::renderTestQuad(ID3D11Device* device, ID3D11DeviceContext* context);
	//void Object3D::createTestQuadGeometryBuffers(ID3D11Device* device, ID3D11DeviceContext* context);

private:
	struct ShaderVariables
	{
		ShaderVariables();
		virtual ~ShaderVariables();

		ID3DX11EffectMatrixVariable* worldView;
		ID3DX11EffectMatrixVariable* worldViewIT; // Inverse-Transposed World View
		ID3DX11EffectMatrixVariable* worldViewProj;
	};

	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;
	static ID3D11InputLayout* s_inputLayout;

	bool readObj(const std::string& path);

	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	uint32_t m_numIndices;

	std::vector<Vertex> m_vertexData;
	std::vector<Triangle> m_indexData;


	//ID3D11Buffer* m_testQuadVB;
	//ID3D11Buffer* m_testQuadIB;
};

#endif