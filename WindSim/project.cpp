#include "project.h"
#include "logger.h"
#include "objectItem.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QByteArray>

// id 0 is the default id for an empty QJsonObject
int Project::s_id = 1;

Project::Project()
	: m_path(),
	m_empty(true),
	m_objectModel()
{
}

Project::~Project()
{
}

bool Project::create()
{
	m_empty = false;
	return true;
}

bool Project::open(const QString& path)
{
	QFile f(path);

	if (!f.open(QIODevice::ReadOnly)) return false;


	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	f.close();

	// JsonObject with 'objects' array
	QJsonObject json = doc.object();
	auto it = json.find("objects");
	if (it == json.end())
	{
		Logger::logit("ERROR: No objects present within the project file '" + path + "'!");
		return false;
	}

	if (!it->isArray())
	{
		Logger::logit("ERROR: Objects not saved as Json-Array within the project file '" + path + "'!");
		return false;
	}

	// Iterate all objects in array
	for (auto i : it->toArray())
	{
		QJsonObject obj = i.toObject();
		addObject(obj);
	}

	m_path = path;
	m_empty = false;
	return true;
}
bool Project::close()
{
	m_objectModel.clear();
	m_path.clear();
	m_empty = true;
	return true;
}

bool Project::save()
{
	return saveAs(m_path);
}
bool Project::saveAs(const QString& path)
{
	// Create Json file from all objects
	int rc = m_objectModel.rowCount();
	QJsonArray objects;
	for (int i = 0; i < rc; ++i)
	{
		QJsonObject json = m_objectModel.item(i)->data().toJsonObject();
		json.remove("id");
		objects.append(json);
	}
	QJsonDocument doc(QJsonObject{ { "objects", objects } });

	QFile f(path);
	if (!f.open(QIODevice::WriteOnly)) return false;
	f.write(doc.toJson());
	f.close();

	m_path = path;
	return true;
}

// Store objects with their propterties as QJsonObjects
// This way, every object can have custom properties, without creating special object classes
// The objects and its properties also may be saved to file in a human readable and modifiable format
ObjectItem* Project::addObject(QJsonObject& data)
{
	if (!verifyObject(data))
	{
		return nullptr;
	}

	ObjectItem* item = new ObjectItem(data["name"].toString());
	data.insert("id", s_id++);
	item->setData(data);
	m_objectModel.appendRow(item);
	return item;
}

QJsonObject Project::removeObject(int id)
{
	ObjectItem* item = findItem(id);
	if (!item)
	{
		Logger::logit("WARNING: Object not removed! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return QJsonObject();
	}

	QJsonObject json = item->data().toJsonObject();
	m_objectModel.removeRow(item->row());

	return json;
}

ObjectItem* Project::modifyObject(QJsonObject& data)
{
	if (!verifyObject(data))
	{
		return nullptr;
	}

	ObjectItem * item = findItem(data["id"].toInt());
	if (!item)
	{
		Logger::logit("WARNING: Object not modified! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return nullptr;
	}
	item->setData(data["name"].toString(), Qt::DisplayRole); // Modify name in gui
	item->setData(data); // Modify object data ( UserRole + 1)
	return item;
}

QJsonObject Project::getObject(int id)
{
	ObjectItem* item = findItem(id);
	if (!item)
	{
		Logger::logit("WARNING: Object not found! The object '" + item->data().toJsonObject()["name"].toString() + "' does not exist.");
		return QJsonObject();
	}

	return item->data().toJsonObject();
}


ObjectItem* Project::findItem(int id)
{
	//Iterate all items in list
	int rc = m_objectModel.rowCount();
	for (int i = 0; i < rc; ++i)
	{
		ObjectItem* item = static_cast<ObjectItem*>(m_objectModel.item(i));
		if (id == item->data().toJsonObject()["id"].toInt()) // Given id equals the id of the current object
		{
			return item;
		}
	}
	return nullptr;
}

bool Project::verifyObject(const QJsonObject& object)
{
	// Check if "name" key exists (necessary for displaying in the list):
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

	// Verify all mandatory keys for meshes exist
	if (type == ObjectType::Mesh)
	{
		auto objIt = object.find("obj-file");
		if (objIt == object.end())
		{
			Logger::logit("WARNING: Found no obj-file for Mesh '" + name + "' in project file!");
			return false;
		}
	}

	return true;
}
