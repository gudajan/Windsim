#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include "object3D.h"
#include "actor.h"

#include <unordered_map>
#include <string>
#include <memory>


enum class ObjectType { Mesh, Sky };


class ObjectManager
{
public:
	ObjectManager();
	~ObjectManager();

	// Add one object, which is rendered
	void add(const std::string& name, ID3D11Device* device, ObjectType type, void const * const data = nullptr);
	// Erase one object
	void erase(const std::string& name);

	// Render ALL objects at their current transformation
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	// Release the DirectX objects of ALL objects
	void release();

private:
	std::unordered_map<std::string, std::unique_ptr<Object3D>> m_objects;
	std::unordered_map<std::string, std::unique_ptr<Actor>> m_actors;
};

#endif