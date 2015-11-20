#include "objectManager.h"
#include "mesh.h"
#include "MeshActor.h"
#include "sky.h"
#include "skyActor.h"

#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

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
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			MeshActor* act = new MeshActor(*obj);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, false);
			act->setAsyncShaderVariables();
		}
		else if (type == ObjectType::Sky)
		{
			Sky* obj = new Sky();
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			SkyActor* act = new SkyActor(*obj);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, true);
		}
		modify(data); // Set initial values from the data
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

void ObjectManager::modify(const QJsonObject& data)
{
	int id = data["id"].toInt();
	ObjectType type = stringToObjectType(data["type"].toString().toStdString());

	std::string name = data["name"].toString().toStdString();

	auto it = m_actors.find(id);
	if (it == m_actors.end())
	{
		throw std::runtime_error("Failed to modify object with id '" + std::to_string(id) + "' as the id was not found!");
	}

	// Modify general data
	it->second->setRender(data["disabled"].toInt() == Qt::Unchecked);

	// Modify object specific data
	if (type == ObjectType::Mesh)
	{
		QJsonObject jPos = data["Position"].toObject();
		XMFLOAT3 pos(jPos["x"].toDouble(), jPos["y"].toDouble(), jPos["z"].toDouble());

		QJsonObject jScale = data["Scaling"].toObject();
		XMFLOAT3 scale(jScale["x"].toDouble(), jScale["y"].toDouble(), jScale["z"].toDouble());

		QJsonObject jRot = data["Rot"].toObject();
		XMFLOAT4 rot;
		XMStoreFloat4(&rot, XMQuaternionRotationRollPitchYaw(jScale["be"].toDouble(), jScale["ga"].toDouble(), jScale["al"].toDouble()));

		bool flatShading = data["Shading"].toString() == "Smooth" ? false : true;

		std::shared_ptr<MeshActor> act = std::dynamic_pointer_cast<MeshActor>(it->second);
		act->setPos(pos);
		act->setScale(scale);
		act->setRot(rot);
		act->setFlatShading(flatShading);
		act->setAsyncShaderVariables();
	}
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