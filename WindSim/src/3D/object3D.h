#ifndef OBJECT_3D_H
#define OBJECT_3D_H

#include <DirectXMath.h>
#include <Windows.h>

#include <vector>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
class Logger;

class Object3D
{
public:
	Object3D(Logger* logger);
	Object3D(Object3D&& other);
	virtual ~Object3D();

	virtual HRESULT create(ID3D11Device* device, bool clearClientBuffers = false);
	virtual void release();

	// Depends on shader variables -> must be reimplemented
	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) = 0;

	// Computes intersection position in object space
	// Returns true if intersection found, otherwise false
	// Assumes data is a triangle list (not applicable for lines or points)
	virtual bool intersect(DirectX::XMFLOAT3& origin, DirectX::XMFLOAT3& direction, float& distance) const;

	virtual void getBoundingBox(DirectX::XMFLOAT3& center, DirectX::XMFLOAT3& extends);

	virtual ID3D11Buffer* getVertexBuffer() { return m_vertexBuffer; };
	virtual ID3D11Buffer* getIndexBuffer() { return m_indexBuffer; };
	virtual uint32_t getNumIndices() const { return m_numIndices; };
	virtual Logger* getLogger() { return m_logger; };

protected:
	virtual void log(const std::string& msg);

	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	uint32_t m_numIndices;

	std::vector<float> m_vertexData;
	std::vector<uint32_t> m_indexData;

	Logger* m_logger;
};

#endif