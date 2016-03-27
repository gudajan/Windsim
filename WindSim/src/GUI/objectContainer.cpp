#include "objectContainer.h"
#include "staticLogger.h"
#include "meshProperties.h"
#include "voxelGridProperties.h"
#include "commands.h"
#include "settings.h"
#include "../3D/simulator.h"

#include <QUndoStack>
#include <QUndoCommand>

extern QUndoStack g_undoStack;

// id 0 is the default id for an empty QJsonObject
int ObjectContainer::s_id = 1;

ObjectContainer::ObjectContainer(QWidget* parent)
	: QObject(parent),
	m_meshProperties(nullptr),
	m_voxelGridProperties(nullptr),
	m_model(),
	m_ids(),
	m_renderer(nullptr),
	m_voxelGridAdded(false)
{
	// Propagate object changes
	connect(&m_model, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(objectsInserted(const QModelIndex &, int, int)));
	connect(&m_model, SIGNAL(rowsAboutToBeRemoved(const QModelIndex &, int, int)), this, SLOT(objectsRemoved(const QModelIndex &, int, int)));
	connect(&m_model, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(objectModified(QStandardItem*)));
}

ObjectContainer::~ObjectContainer()
{
}

void ObjectContainer::clear()
{
	m_model.clear();
	m_ids.clear();
	//m_meshProperties->close();
	m_voxelGridAdded = false;
	emit removeAllObject3DTriggered(); // Remove objects in renderer
}


QJsonObject ObjectContainer::getData(int id)
{
	ObjectItem* item = getItem(id);
	if (!item)
	{
		StaticLogger::logit("WARNING: Object not found! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return QJsonObject();
	}

	return item->data().toJsonObject();
}


ObjectItem* ObjectContainer::getItem(int id)
{
	// count may be 0 or 1 for sets
	if (!m_ids.count(id))
	{
		return nullptr;
	}
	//Iterate all items in list
	int rc = m_model.rowCount();
	for (int i = 0; i < rc; ++i)
	{
		ObjectItem* item = static_cast<ObjectItem*>(m_model.item(i));
		if (id == item->data().toJsonObject()["id"].toInt()) // Given id equals the id of the current object
		{
			return item;
		}
	}
	return nullptr;
}

void ObjectContainer::setRenderer(DX11Renderer* renderer)
{
	m_renderer = renderer;

	// Connect changes in GUI to renderer
	connect(this, &ObjectContainer::addObject3DTriggered, m_renderer, &DX11Renderer::onAddObject);
	connect(this, &ObjectContainer::modifyObject3DTriggered, m_renderer, &DX11Renderer::onModifyObject);
	connect(this, &ObjectContainer::removeObject3DTriggered, m_renderer, &DX11Renderer::onRemoveObject);
	connect(this, &ObjectContainer::removeAllObject3DTriggered, m_renderer, &DX11Renderer::onRemoveAll);
	connect(this, &ObjectContainer::triggerFunction, m_renderer, &DX11Renderer::onTriggerFunction);

	// Connect changes from renderer to this container(and the properties dialogs)
	connect(m_renderer, &DX11Renderer::modify, this, &ObjectContainer::rendererModification);
}

void ObjectContainer::showPropertiesDialog(const QModelIndex& index)
{
	QJsonObject data = m_model.data(index, Qt::UserRole + 1).toJsonObject();
	ObjectType type = stringToObjectType(data["type"].toString().toStdString());

	if (type == ObjectType::Mesh)
	{

		if (!m_meshProperties)
		{
			m_meshProperties = new MeshProperties(data, static_cast<QWidget*>(parent())); // Save as we know our parent is a QWidget
			m_meshProperties->setup(this);
		}
		m_meshProperties->updateProperties(data);
		m_meshProperties->show();
		m_meshProperties->raise();
	}
	else if (type == ObjectType::VoxelGrid)
	{
		if (!m_voxelGridProperties)
		{
			m_voxelGridProperties = new VoxelGridProperties(data, static_cast<QWidget*>(parent()));
			m_voxelGridProperties->setup(this);
		}
		m_voxelGridProperties->updateProperties(data);
		m_voxelGridProperties->show();
		m_voxelGridProperties->raise();
	}
}

void ObjectContainer::addCmd(const QJsonObject& data)
{
	if (stringToObjectType(data["type"].toString().toStdString()) == ObjectType::VoxelGrid)
	{
		if (m_voxelGridAdded)
		{
			StaticLogger::logit("WARNING: Currently, multiple VoxelGrids are not supported. Creation aborted!");
			return;
		}
		m_voxelGridAdded = true;
	}
	QUndoCommand* cmd = new AddObjectCmd(data, this);
	g_undoStack.push(cmd);
}

void ObjectContainer::removeCmd(int id)
{
	QUndoCommand* cmd = new RemoveObjectCmd(id, this);
	g_undoStack.push(cmd);
}

void ObjectContainer::modifyCmd(const QJsonObject& data, Modifications mod)
{
	ObjectItem* item = getItem(data["id"].toInt());

	QUndoCommand* cmd = new ModifyObjectCmd(item->data().toJsonObject(), data, item, mod);
	g_undoStack.push(cmd);
}

void ObjectContainer::rendererModification(std::vector<QJsonObject> data)
{
	QUndoCommand * transformation = new QUndoCommand();
	transformation->setText("Modify via 3D view");

	for (const auto& json : data)
	{
		ObjectItem* item = getItem(json["id"].toInt());
		new ModifyObjectCmd(item->data().toJsonObject(), json, item, Position | Scaling | Rotation, transformation); // Add command to command group
	}

	g_undoStack.push(transformation);
}

void ObjectContainer::objectsInserted(const QModelIndex & parent, int first, int last)
{
	// Add Object3D for every inserted Item in the object List, using the specified data
	for (int i = first; i <= last; ++i)
		emit addObject3DTriggered(m_model.item(i)->data().toJsonObject());
}

void ObjectContainer::objectsRemoved(const QModelIndex & parent, int first, int last)
{
	// Remove Object3D for every item, that is about to be removed
	// Hide all properties dialogs if respective object was deleted:
	for (int i = first; i <= last; ++i)
	{
		const QJsonObject& json = m_model.item(i)->data().toJsonObject();
		int id = json["id"].toInt();
		emit removeObject3DTriggered(id);
		if (m_meshProperties && id == m_meshProperties->getCurrentID()) // Dialog already exists and is for removed object
			m_meshProperties->hide();
		if (m_voxelGridProperties && id == m_voxelGridProperties->getCurrentID()) // Dialog already exists and is for removed object
			m_voxelGridProperties->hide();

		if (stringToObjectType(json["type"].toString().toStdString()) == ObjectType::VoxelGrid)
			m_voxelGridAdded = false;
	}
}

void ObjectContainer::objectModified(QStandardItem * item)
{
	QJsonObject data = item->data().toJsonObject();
	// Trigger update for renderer
	emit modifyObject3DTriggered(data);
	// Trigger update for all properties dialogs
	emit updatePropertiesDialog(data);
}

bool ObjectContainer::verifyData(QJsonObject& object)
{
	// Verify mandatory keys for all types (name, type):
	auto nameIt = object.find("name");
	if (nameIt == object.end())
	{
		StaticLogger::logit("WARNING: Found no name for object in project file!");
		return false;
	}
	const QString& name = nameIt->toString();

	// Check if "type" key exists:
	auto typeIt = object.find("type");
	if (typeIt == object.end())
	{
		StaticLogger::logit("WARNING: Found no type for object '" + name + "' in project file!");
		return false;
	}

	ObjectType type = stringToObjectType(typeIt->toString().toStdString());

	// Insert all non-existent keys for all types with default values,
	if (!object.contains("id"))
	{
		object.insert("id", s_id);
		m_ids.insert(s_id++);
	}
	if (!object.contains("disabled"))
		object["disabled"] = Qt::Unchecked;

	if (type == ObjectType::Mesh)
	{
		// Verify mandatory keys for meshes
		if (!object.contains("obj-file"))
		{
			StaticLogger::logit("WARNING: Found no obj-file for Mesh '" + name + "' in project file!");
			return false;
		}

		// Insert all non-existent keys for meshes with default values
		if (!object.contains("Position"))
			object["Position"] = QJsonObject{ { "x", 0.0 }, { "y", 0.0 }, { "z", 0.0 } };
		if (!object.contains("Scaling"))
			object["Scaling"] = QJsonObject{ { "x", 1.0 }, { "y", 1.0 }, { "z", 1.0 } };
		if (!object.contains("Rotation"))
			object["Rotation"] = QJsonObject{ { "ax", 0.0 }, { "ay", 1.0 }, { "az", 0.0 }, { "angle", 0.0 } };
		if (!object.contains("Shading"))
			object["Shading"] = "Flat";
		if (!object.contains("Color"))
			object["Color"] = QJsonObject{ { "r", conf.mesh.dc.r }, { "g", conf.mesh.dc.g }, { "b", conf.mesh.dc.b } };
		if (!object.contains("voxelize"))
			object["voxelize"] = Qt::Checked;
		if (!object.contains("dynamics"))
			object["dynamics"] = Qt::Checked;
		if (!object.contains("density"))
			object["density"] = 2712.0; // Density of aluminium, Iron: 7850, Steel: 7850, Brass 60/40: 8520
		if (!object.contains("localRotAxis"))
			object["localRotAxis"] = QJsonObject{ { "enabled", Qt::Unchecked }, { "x", 0.0f }, { "y", 0.0f }, { "z", 0.0f } };
		if (!object.contains("showAccelArrow"))
			object["showAccellArrow"] = Qt::Unchecked;
	}
	if (type == ObjectType::VoxelGrid)
	{
		// Verify mandatory keys for voxel grids
		if (!object.contains("resolution"))
		{
			StaticLogger::logit("WARNING: Found no resolution for Voxel Grid '" + name + "' in project file!");
			return false;
		}
		if (!object.contains("voxelSize"))
		{
			StaticLogger::logit("WARNING: Found no voxel size for Voxel Grid '" + name + "' in project file!");
			return false;
		}

		// Insert all non-existent keys for voxel grids with default values
		if (!object.contains("Position"))
			object["Position"] = QJsonObject{ { "x", 0.0 }, { "y", 0.0 }, { "z", 0.0 } };
		if (!object.contains("Scaling"))
			object["Scaling"] = QJsonObject{ { "x", 1.0 }, { "y", 1.0 }, { "z", 1.0 } };
		if (!object.contains("Rotation"))
			object["Rotation"] = QJsonObject{ { "ax", 0.0 }, { "ay", 1.0 }, { "az", 0.0 }, { "angle", 0.0 } };
		if (!object.contains("renderVoxel"))
			object["renderVoxel"] = Qt::Unchecked;
		if (!object.contains("glyphs"))
		{
			QJsonObject tmp{ { "x", object["resolution"].toObject()["x"].toInt() }, { "y", object["resolution"].toObject()["y"].toInt() } };
			object["glyphs"] = QJsonObject{ { "enabled", false }, { "orientation", XY_PLANE }, { "position", 0.5}, {"quantity", tmp} };
		}
		if (!object.contains("windtunnelSettings"))
			object["windtunnelSettings"] = ""; // Simulation uses default values
		if (!object.contains("runSimulation"))
			object["runSimulation"] = Qt::Unchecked;
		if (!object.contains("smoke"))
		{
			object["smoke"] = Simulator::getSmokeSettingsDefault();
		}
		if (!object.contains("lines"))
			object["lines"] = Simulator::getLineSettingsDefault();
	}

	return true;
}

// Store objects with their propterties as QJsonObjects
// This way, every object can have custom properties, without creating special object classes
// The objects and its properties also may be saved to file in a human readable and modifiable format
bool ObjectContainer::add(ObjectType type, QJsonObject& data)
{
	if (!verifyData(data)) return false;

	ObjectItem* item = new ObjectItem(data["name"].toString());
	item->setData(data);
	m_model.appendRow(item);
	return true;
}

bool ObjectContainer::remove(int id)
{
	ObjectItem* item = getItem(id);
	if (!item)
	{
		StaticLogger::logit("WARNING: Object not removed! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return false;
	}

	QJsonObject json = item->data().toJsonObject();
	m_model.removeRow(item->row());
	m_ids.erase(json["id"].toInt());

	return true;
}

bool ObjectContainer::modify(QJsonObject& data)
{
	if (!verifyData(data)) return false;

	ObjectItem * item = getItem(data["id"].toInt());
	if (!item)
	{
		StaticLogger::logit("WARNING: Object not modified! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return false;
	}
	item->setData(data["name"].toString(), Qt::DisplayRole); // Modify name in gui
	item->setData(data); // Modify object data ( UserRole + 1)

	return true;
}