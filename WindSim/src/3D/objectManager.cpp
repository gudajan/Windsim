#include "objectManager.h"
#include "mesh3D.h"
#include "MeshActor.h"
#include "sky.h"
#include "skyActor.h"
#include "axes.h"
#include "axesActor.h"
#include "voxelGrid.h"
#include "voxelGridActor.h"
#include "logger.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <limits>

using namespace DirectX;

ObjectManager::ObjectManager(Logger* logger)
	: m_hoveredId(0),
	m_selectedIds(),
	m_objects(),
	m_actors(),
	m_accessoryObjects(),
	m_accessoryActors(),
	m_logger(logger)
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
			Mesh3D* obj = new Mesh3D(objIt->toString().toStdString(), m_logger);
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			MeshActor* act = new MeshActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, false);
			act->create(device);
		}
		else if (type == ObjectType::Sky)
		{
			Sky* obj = new Sky(m_logger);
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			SkyActor* act = new SkyActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, true);
		}
		else if (type == ObjectType::Axes)
		{
			Axes* obj = new Axes(m_logger);
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			AxesActor* act = new AxesActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, true);
		}
		else if (type == ObjectType::VoxelGrid)
		{
			if (!data.contains("resolution") || !data.contains("voxelSize"))
				throw std::invalid_argument("Failed to create VoxelGrid object '" + name + "' because no resolution or voxelSize was given in 'data' variable!");

			QJsonObject resolution = data["resolution"].toObject();
			XMUINT3 res(resolution["x"].toInt(), resolution["y"].toInt(), resolution["z"].toInt());
			QJsonObject voxelSize = data["voxelSize"].toObject();
			XMFLOAT3 vs(voxelSize["x"].toDouble(), voxelSize["y"].toDouble(), voxelSize["z"].toDouble());

			VoxelGrid* obj = new VoxelGrid(this, res, vs, data["simulator"].toString().toStdString(), m_logger);
			m_objects.emplace(id, std::shared_ptr<Object3D>(obj));
			VoxelGridActor* act = new VoxelGridActor(*obj, id);
			m_actors.emplace(id, std::shared_ptr<Actor>(act));

			obj->create(device, false);
		}
		else
		{
			throw std::runtime_error("Object '" + name + "' has type Invalid!");
		}
		modify(data); // Set initial values from the data
	}
	else
	{
		throw std::runtime_error("Failed to add object '" + name + "' with id " + std::to_string(id) + " as object with identical id already exists!");
	}
}

void ObjectManager::remove(int id)
{
	const auto object = m_objects.find(id);
	if (object == m_objects.end())
		throw std::runtime_error("Failed to remove object with id '" + std::to_string(id) + "' as the id was not found!");

	object->second->release();
	m_objects.erase(id);
	m_actors.erase(id);

}

void ObjectManager::removeAll()
{
	release(false);
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
	bool render = data["disabled"].toInt() == Qt::Unchecked;
	bool oldRender = it->second->getRender();
	it->second->setRender(render);

	// Modify object specific data
	if (type == ObjectType::Mesh)
	{
		QJsonObject jPos = data["Position"].toObject();
		XMFLOAT3 pos(jPos["x"].toDouble(), jPos["y"].toDouble(), jPos["z"].toDouble());

		QJsonObject jScale = data["Scaling"].toObject();
		XMFLOAT3 scale(jScale["x"].toDouble(), jScale["y"].toDouble(), jScale["z"].toDouble());

		QJsonObject jRot = data["Rotation"].toObject();
		XMFLOAT4 rot;
		XMVECTOR axis = XMVectorSet(jRot["ax"].toDouble(), jRot["ay"].toDouble(), jRot["az"].toDouble(), 0.0);
		if (XMVector3Equal(axis, XMVectorZero()))
			XMStoreFloat4(&rot, XMQuaternionIdentity());
		else
			XMStoreFloat4(&rot, XMQuaternionRotationAxis(axis, degToRad(jRot["angle"].toDouble())));

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
		act->setVoxelize(data["voxelize"].toInt() == Qt::Checked);

		if (render != oldRender && !render)
		{
			m_selectedIds.erase(act->getId());
			act->setSelected(false);
		}

	}
	else if (type == ObjectType::VoxelGrid)
	{
		QJsonObject jPos = data["Position"].toObject();
		XMFLOAT3 pos(jPos["x"].toDouble(), jPos["y"].toDouble(), jPos["z"].toDouble());

		QJsonObject jRes = data["resolution"].toObject();
		XMUINT3 res(jRes["x"].toInt(), jRes["y"].toInt(), jRes["z"].toInt());

		QJsonObject jVs = data["voxelSize"].toObject();
		XMFLOAT3 vs(jVs["x"].toDouble(), jVs["y"].toDouble(), jVs["z"].toDouble());


		std::shared_ptr<VoxelGridActor> act = std::dynamic_pointer_cast<VoxelGridActor>(it->second);
		act->setPos(pos);
		act->computeWorld();
		act->resize(res, vs);
		act->setRenderVoxel(data["renderVoxel"].toInt() == Qt::Checked);
		act->setSimulator(data["simulator"].toString().toStdString());
	}
}

void ObjectManager::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	for (const auto& actor : m_actors)
	{
		actor.second->render(device, context, view, projection);
	}
	for (const auto& actor : m_accessoryActors)
	{
		actor.second->render(device, context, view, projection);
	}
}

void ObjectManager::release(bool withAccessories)
{
	for (const auto& object : m_objects)
	{
		object.second->release();
	}
	for (const auto& actor : m_actors)
	{
		if (actor.second->getType() == ObjectType::Mesh)
		{
			std::dynamic_pointer_cast<MeshActor>(actor.second)->release();
		}
	}

	if (withAccessories)
	{
		for (const auto& object : m_accessoryObjects)
		{
			object.second->release();
		}
	}
}


void ObjectManager::voxelizeNextFrame()
{
	for (const auto& act : m_actors)
	{
		if (act.second->getType() == ObjectType::VoxelGrid)
			std::dynamic_pointer_cast<VoxelGridActor>(act.second)->voxelize();
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
		bool changed = ids != m_selectedIds;
		if (changed)
			setSelected();
		return changed;
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

void ObjectManager::addAccessoryObject(const std::string& name, std::shared_ptr<Object3D> obj, std::shared_ptr<Actor> act)
{
	bool inserted = m_accessoryObjects.emplace(name, obj).second;
	if (!inserted)
		throw std::runtime_error("Failed to add accessory object '" + name + "' as identically named object already exists!");
	m_accessoryActors.emplace(name, act);
}

int ObjectManager::computeIntersection(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float& distance) const
{
	float closestDist = std::numeric_limits<float>::infinity(); // The distance to the intersection, closest to the camera
	int closestID = 0;

	// Search for intersection, closes to the camera
	for (auto& act : m_actors)
	{
		// Only enabled meshes are searched for intersections
		if (act.second->getType() == ObjectType::Mesh && act.second->getRender())
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
		std::shared_ptr<Actor> a = m_actors.find(id)->second;
		if (a->getType() == ObjectType::Mesh && a->getRender())
			m_selectedIds.insert(id);
	}
}

void ObjectManager::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
}