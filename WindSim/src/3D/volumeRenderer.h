#ifndef VOLUME_RENDERER_H
#define VOLUME_RENDERER_H

#include <string>
#include <Windows.h>
#include <DirectXMath.h>

class DX11Renderer;

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture3D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;


struct ID3DX11Effect;
struct ID3DX11EffectShaderResourceVariable;
struct ID3DX11EffectVectorVariable;
struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectUnorderedAccessViewVariable;

class VolumeRenderer
{
public:
	static HRESULT createShaderFromFile(const std::wstring& path, ID3D11Device* device, const bool reload = false);
	static void releaseShader();

	VolumeRenderer();

	HRESULT create(ID3D11Device* device, const DirectX::XMUINT3 gridDimensions);
	void release();

	void render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* vectorGrid, ID3D11ShaderResourceView* depthStencilView, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, const DirectX::XMUINT3 gridDimensions, const DirectX::XMFLOAT3& voxelSize);

private:
	struct ShaderVariables
	{
		ID3DX11EffectUnorderedAccessViewVariable* scalarGridUAV;
		ID3DX11EffectShaderResourceVariable* scalarGridSRV;
		ID3DX11EffectShaderResourceVariable* vectorGrid;
		ID3DX11EffectShaderResourceVariable* depthTex;

		ID3DX11EffectMatrixVariable* proj;
		ID3DX11EffectMatrixVariable* projInv;
		ID3DX11EffectMatrixVariable* worldView;
		ID3DX11EffectMatrixVariable* worldViewProjInv;
		ID3DX11EffectMatrixVariable* texToGrid;
		ID3DX11EffectMatrixVariable* texToGridInv;

		ID3DX11EffectVectorVariable* dimensions;
		ID3DX11EffectVectorVariable* camPosTS;
	};
	static ShaderVariables s_shaderVariables;
	static ID3DX11Effect* s_effect;

	ID3D11Texture3D* m_scalarGrid;
	ID3D11UnorderedAccessView* m_scalarGridUAV;
	ID3D11ShaderResourceView* m_scalarGridSRV;
};


#endif