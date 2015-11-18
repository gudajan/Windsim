#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include "object3D.h"
#include "actor.h"
#include "common.h"

#include <unordered_map>
#include <memory>

#include <QJsonObject>


class ObjectManager
{
public:
	ObjectManager();
	~ObjectManager();

	// Add one object, which is rendered
	void add(ID3D11Device* device, const QJsonObject& data);
	// Remove one object
	void remove(int id);
	void removeAll();

	// Render ALL objects at their current transformation
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	// Release the DirectX objects of ALL objects
	void release();

private:
	std::unordered_map<int, std::unique_ptr<Object3D>> m_objects;
	std::unordered_map<int, std::unique_ptr<Actor>> m_actors;
};

#endif