#ifndef ACTOR_H
#define ACTOR_H

#include "common.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>

class ID3D11Device;
class ID3D11DeviceContext;

class Actor
{
public:
	Actor(ObjectType type, int id);
	virtual ~Actor();

	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) = 0;

	virtual bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float& distance) const;

	virtual void setPos(const DirectX::XMFLOAT3& pos);
	virtual void setRot(const DirectX::XMFLOAT4& rot);
	virtual void setScale(const DirectX::XMFLOAT3& scale);
	virtual void computeWorld();
	virtual void setRender(bool render);
	virtual void toggleRender();

	virtual const DirectX::XMFLOAT3& getPos() const;
	virtual const DirectX::XMFLOAT4& getRot() const;
	virtual const DirectX::XMFLOAT3& getScale() const;
	virtual DirectX::XMFLOAT4X4 getWorld() const;
	ObjectType getType() const { return m_type; };
	int getId() const { return m_id; };

protected:
	DirectX::XMFLOAT3 m_pos;
	DirectX::XMFLOAT4 m_rot;
	DirectX::XMFLOAT3 m_scale;
	DirectX::XMFLOAT4X4 m_world; // object to world space
	bool m_render;
	ObjectType m_type;
	int m_id;
};

#endif