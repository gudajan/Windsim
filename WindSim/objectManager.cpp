#include "objectManager.h"
#include "mesh.h"
#include "MeshActor.h"
#include "sky.h"
#include "skyActor.h"

#include <d3d11.h>

ObjectManager::ObjectManager()
	: m_objects(),
	m_actors()
{
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::add(ID3D11Device* device, const QJsonObject& data)
{
	int id = data["id"].toInt();
	ObjectType type = stringToObjectType(data["type"].toString().toStdString());
	const std::string& name = data["name"].toString().toStdString();

	if (m_objects.find(id) == m_objects.end() && m_actors.find(id) == m_actors.end())
	{
		if (type == ObjectType::Mesh)
		{
			auto objIt = data.find("obj-file");
			if (objIt == data.end())
			{
				throw std::invalid_argument("Failed to create Mesh object '" + name + "' because no OBJ-Path was given in 'data' variable!");
			}
			Mesh* obj = new Mesh(objIt->toString().toStdString());
			m_objects.emplace(id, std::unique_ptr<Object3D>(obj));
			MeshActor* act = new MeshActor(*obj);
			m_actors.emplace(id, std::unique_ptr<Actor>(act));

			obj->create(device, false);

		}
		else if (type == ObjectType::Sky)
		{
			Sky* obj = new Sky();
			m_objects.emplace(id, std::unique_ptr<Object3D>(obj));
			SkyActor* act = new SkyActor(*obj);
			m_actors.emplace(id, std::unique_ptr<Actor>(act));

			obj->create(device, true);
		}
	}
	else
	{
		throw std::runtime_error("Failed to add object '" + name + "' as identically named object already exists!");
	}
}

void ObjectManager::remove(int id)
{
	const auto object = m_objects.find(id);
	if (object != m_objects.end())
	{
		object->second->release();
		m_objects.erase(id);
	}
	else
	{
		throw std::runtime_error("Failed to remove object with id '" + std::to_string(id) + "' as the id was not found!");
	}
	m_actors.erase(id);
}

void ObjectManager::removeAll()
{
	release();
	m_objects.clear();
	m_actors.clear();
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