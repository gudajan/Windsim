#include "objectManager.h"
#include "mesh.h"
#include "MeshActor.h"

#include <d3d11.h>

ObjectManager::ObjectManager()
	: m_objects(),
	m_actors()
{
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::add(const std::string& name, ID3D11Device* device, const std::string& path, ObjectType type)
{
	if (type == ObjectType::Mesh)
	{
		if (m_objects.find(name) == m_objects.end() && m_actors.find(name) == m_actors.end())
		{
			Mesh* obj = new Mesh(path);
			m_objects.emplace(name, std::unique_ptr<Object3D>(obj));
			MeshActor* act = new MeshActor(*obj);
			m_actors.emplace(name, std::unique_ptr<Actor>(act));

			obj->create(device, false);
		}
		else
		{
			throw std::runtime_error("Failed to add object '" + name + "' as identically named object already exists!");
		}
	}
}

void ObjectManager::erase(const std::string& name)
{
	const auto object = m_objects.find(name);
	if (object != m_objects.end())
	{
		object->second->release();
		m_objects.erase(name);
	}
	m_actors.erase(name);
}

void ObjectManager::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	for (const auto& actor : m_actors)
	{
		actor.second->render(device, context, view, projection);
	}
}

void ObjectManager::release()
{
	for (const auto& object : m_objects)
	{
		object.second->release();
	}
}