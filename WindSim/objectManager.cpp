#include "objectManager.h"
#include "mesh3D.h"
#include "MeshActor.h"
#include "sky.h"
#include "skyActor.h"
#include "axes.h"
#include "axesActor.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <limits>

using namespace DirectX;

ObjectManager::ObjectManager()
	: m_hoveredId(0),
	m_selectedIds(),
	m_objects(),
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
			Mesh3D* obj = new Mesh3D(objIt->toString().toStdString());
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			MeshActor* act = new MeshActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, false);
		}
		else if (type == ObjectType::Sky)
		{
			Sky* obj = new Sky();
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			SkyActor* act = new SkyActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, true);
		}
		else if (type == ObjectType::Axes)
		{
			Axes* obj = new Axes();
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			AxesActor* act = new AxesActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<AxesActor>(act));

			obj->create(device, true);
		}
		else
		{
			throw std::runtime_error("Object '" + name + "' has type Invalid!");
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

		QJsonObject jRot = data["Rotation"].toObject();
		XMFLOAT4 rot;
		XMStoreFloat4(&rot, XMQuaternionRotationRollPitchYaw(degToRad(jRot["be"].toDouble()), degToRad(jRot["ga"].toDouble()), degToRad(jRot["al"].toDouble())));

		bool flatShading = data["Shading"].toString() == "Smooth" ? false : true;

		QJsonObject jCol = data["Color"].toObject();
		PackedVector::XMCOLOR col(jCol["r"].toInt() / 255.0f, jCol["g"].toInt() / 255.0f, jCol["b"].toInt() / 255.0f, 1.0f);

		std::shared_ptr<MeshActor> act = std::dynamic_pointer_cast<MeshActor>(it->second);
		act->setPos(pos);
		act->setScale(scale);
		act->setRot(rot);
		act->computeWorld();
		act->setFlatShading(flatShading);
		act->setColor(col);
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

void ObjectManager::updateCursor(const XMFLOAT3& origin, const XMFLOAT3& direction, bool containsCursor)
{
	float distance = std::numeric_limits<float>::infinity();
	if (containsCursor) // Only check for intersection if cursor within 3D view
		m_hoveredId = computeIntersection(origin, direction, distance);
	else
		m_hoveredId = 0;
}

bool ObjectManager::updateSelection(Selection op)
{
	if (m_hoveredId) // Only change selection if clicked onto an object
	{
		std::unordered_set<int> ids = m_selectedIds;
		if (op == Selection::Replace) // No modifier
		{
			m_selectedIds.clear();
			m_selectedIds.insert(m_hoveredId);
		}
		else if (op == Selection::Switch) // Ctrl key is pressed
		{
			// If hovered id not removed from selection: insert it; otherwise: id removed as necessary
			if (!m_selectedIds.erase(m_hoveredId))
				m_selectedIds.insert(m_hoveredId);
		}
		else if (op == Selection::Clear)
		{
			m_selectedIds.clear();
		}
		return ids != m_selectedIds;
	}
	return false;
}

void ObjectManager::setHovered()
{
	// Set all Meshes to not hovered
	for (auto& act : m_actors)
	{
		if (act.second->getType() == ObjectType::Mesh)
		{
			std::dynamic_pointer_cast<MeshActor>(act.second)->setHovered(false);
		}
	}

	// If intersection found and its at a mesh -> set to be hovered
	auto& act = m_actors.find(m_hoveredId);
	if (act != m_actors.end() && act->second->getType() == ObjectType::Mesh)
		std::dynamic_pointer_cast<MeshActor>(act->second)->setHovered(true);
}

int ObjectManager::computeIntersection(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float& distance) const
{
	float closestDist = std::numeric_limits<float>::infinity(); // The distance to the intersection, closest to the camera
	int closestID = 0;

	// Search for intersection, closes to the camera
	for (auto& act : m_actors)
	{
		// Only meshes are searched for intersections
		if (act.second->getType() == ObjectType::Mesh)
		{
			float dist = std::numeric_limits<float>::infinity();
			if (std::dynamic_pointer_cast<MeshActor>(act.second)->intersect(origin, direction, dist)) // If intersected -> maybe hovered
			{
				if (dist < closestDist) // If new intersection is closer than closest intersection up to now
				{
					closestDist = dist;
					closestID = act.first;
				}
			}
		}
	}
	distance = closestDist;
	return closestID;
}


void ObjectManager::setSelected()
{
	// Set made selection
	for (auto& act : m_actors)
	{
		if (act.second->getType() == ObjectType::Mesh)
		{
			std::dynamic_pointer_cast<MeshActor>(act.second)->setSelected(false);
		}
	}

	for (int id : m_selectedIds)
	{
		std::dynamic_pointer_cast<MeshActor>(m_actors.find(id)->second)->setSelected(true); // All ids belong to meshes
	}
}

const void ObjectManager::setSelection(const std::unordered_set<int>& selection)
{
	m_selectedIds.clear();

	// Check which ids belong to meshes and add them if so
	for (int id : selection)
	{
		if (m_actors.find(id)->second->getType() == ObjectType::Mesh)
			m_selectedIds.insert(id);
	}
}