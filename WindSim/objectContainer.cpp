#include "objectContainer.h"
#include "logger.h"
#include "meshProperties.h"
#include "commands.h"
#include "dx11renderer.h"
#include "settings.h"

#include <QUndoStack>
#include <QUndoCommand>

extern QUndoStack g_undoStack;

// id 0 is the default id for an empty QJsonObject
int ObjectContainer::s_id = 1;

ObjectContainer::ObjectContainer(QWidget* parent)
	: QObject(parent),
	m_meshProperties(nullptr),
	m_model(),
	m_ids(),
	m_renderer(nullptr)
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
	emit removeAllObject3DTriggered(); // Remove objects in renderer
}


QJsonObject ObjectContainer::getData(int id)
{
	ObjectItem* item = getItem(id);
	if (!item)
	{
		Logger::logit("WARNING: Object not found! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
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

	connect(this, &ObjectContainer::addObject3DTriggered, m_renderer, &DX11Renderer::onAddObject);
	connect(this, &ObjectContainer::modifyObject3DTriggered, m_renderer, &DX11Renderer::onModifyObject);
	connect(this, &ObjectContainer::removeObject3DTriggered, m_renderer, &DX11Renderer::onRemoveObject);
	connect(this, &ObjectContainer::removeAllObject3DTriggered, m_renderer, &DX11Renderer::onRemoveAll);

	// TODO connect changes from renderer to this container(and the properties dialogs)
}

void ObjectContainer::showPropertiesDialog(const QModelIndex& index)
{
	QJsonObject data = m_model.data(index, Qt::UserRole + 1).toJsonObject();

	if (!m_meshProperties)
	{
		m_meshProperties = new MeshProperties(data, static_cast<QWidget*>(parent())); // Save as we know our parent is a QWidget
		m_meshProperties->setup(this);
	}

	m_meshProperties->updateProperties(data);

	m_meshProperties->show();
	m_meshProperties->raise();
}

void ObjectContainer::addCmd(const QJsonObject& data)
{
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
		int id = m_model.item(i)->data().toJsonObject()["id"].toInt();
		emit removeObject3DTriggered(id);
		if (id == m_meshProperties->getCurrentID())
			m_meshProperties->hide();
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
		Logger::logit("WARNING: Found no name for object in project file!");
		return false;
	}
	const QString& name = nameIt->toString();

	// Check if "type" key exists:
	auto typeIt = object.find("type");
	if (typeIt == object.end())
	{
		Logger::logit("WARNING: Found no type for object '" + name + "' in project file!");
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
			Logger::logit("WARNING: Found no obj-file for Mesh '" + name + "' in project file!");
			return false;
		}

		// Insert all non-existent keys for meshes with default values
		if (!object.contains("Position"))
			object["Position"] = QJsonObject{ { "x", 0.0 }, { "y", 0.0 }, { "z", 0.0 } };
		if (!object.contains("Scaling"))
			object["Scaling"] = QJsonObject{ { "x", 1.0 }, { "y", 1.0 }, { "z", 1.0 } };
		if (!object.contains("Rotation"))
			object["Rotation"] = QJsonObject{ { "al", 0.0 }, { "be", 0.0 }, { "ga", 0.0 } };
		if (!object.contains("Shading"))
			object["Shading"] = "Flat";
		if (!object.contains("Color"))
			object["Color"] = QJsonObject{ { "r", conf.mesh.dc.r }, { "g", conf.mesh.dc.g }, { "b", conf.mesh.dc.b } };
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
	item->setEnabled(data["disabled"].toInt() == Qt::Unchecked); // Indicate if object is currently enabled/rendered
	item->setData(data);
	m_model.appendRow(item);
	return true;
}

bool ObjectContainer::remove(int id)
{
	ObjectItem* item = getItem(id);
	if (!item)
	{
		Logger::logit("WARNING: Object not removed! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
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
		Logger::logit("WARNING: Object not modified! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return false;
	}
	item->setData(data["name"].toString(), Qt::DisplayRole); // Modify name in gui
	item->setEnabled(data["disabled"].toInt() == Qt::Unchecked); // Indicate if object is currently enabled/rendered
	item->setData(data); // Modify object data ( UserRole + 1)

	return true;
}