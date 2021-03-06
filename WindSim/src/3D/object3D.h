#ifndef OBJECT_3D_H
#define OBJECT_3D_H

#include <DirectXMath.h>
#include <Windows.h>

#include <vector>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct DXGI_SURFACE_DESC;
class DX11Renderer;

class Object3D
{
public:
	Object3D(DX11Renderer* renderer);
	virtual ~Object3D() = default;

	virtual HRESULT create(ID3D11Device* device, bool clearClientBuffers = false);
	virtual void release();

	// Depends on shader variables -> pure virtual -> must be reimplemented
	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) = 0;

	// Called when render target is resized
	// default implementation does nothing
	virtual void onResizeSwapChain(ID3D11Device* device, const DXGI_SURFACE_DESC* backBufferDesc) {};

	// Computes intersection position in object space
	// Returns true if intersection found, otherwise false
	// Assumes data is a triangle list (not applicable for lines or points)
	virtual bool intersect(DirectX::XMFLOAT3& origin, DirectX::XMFLOAT3& direction, float& distance) const;

	virtual void getBoundingBox(DirectX::XMFLOAT3& center, DirectX::XMFLOAT3& extends);

	virtual ID3D11Buffer* getVertexBuffer() { return m_vertexBuffer; };
	virtual ID3D11Buffer* getIndexBuffer() { return m_indexBuffer; };
	virtual uint32_t getNumIndices() const { return m_numIndices; };

	virtual void log(const std::string& msg);

protected:
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	uint32_t m_numIndices;

	std::vector<float> m_vertexData;
	std::vector<uint32_t> m_indexData;

	DX11Renderer* m_renderer;

};

#endif