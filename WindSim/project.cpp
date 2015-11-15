#include "project.h"
#include "logger.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QByteArray>

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

	// Iterate all objects
	for (auto i : it->toArray())
	{
		QJsonObject obj = i.toObject();
		QString name;
		if (!verifyObject(obj, name))
			continue;
		QStandardItem* item = new QStandardItem(name);
		item->setData(obj);
		m_objectModel.appendRow(item);
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
		objects.append(m_objectModel.item(i)->data().toJsonObject());
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
bool Project::addObject(const QJsonObject& data)
{
	QString name;
	if (!verifyObject(data, name))
	{
		return false;
	}

	QStandardItem* item = new QStandardItem(name);
	item->setData(data);
	m_objectModel.appendRow(item);

	return true;
}

QJsonObject Project::removeObject(const QString& name)
{
	QStandardItem* item = findItem(name);
	if (!item)
	{
		Logger::logit("WARNING: Object not removed! The object '" + name + "' does not exist.");
		return QJsonObject();
	}

	QJsonObject json = item->data().toJsonObject();
	m_objectModel.removeRow(item->row());

	return json;
}

QJsonObject Project::getObject(const QString& name)
{
	QStandardItem* item = findItem(name);
	if (!item)
	{
		Logger::logit("WARNING: Object not found! The object '" + name + "' does not exist.");
		return QJsonObject();
	}

	return item->data().toJsonObject();
}


QStandardItem* Project::findItem(const QString& name)
{
	//Iterate all items in list
	int rc = m_objectModel.rowCount();
	for (int i = 0; i < rc; ++i)
	{
		QStandardItem* item = m_objectModel.item(i);
		if (name == item->text()) // Displayed text equals the searched name
		{
			return item;
		}
	}
	return nullptr;
}

bool Project::verifyObject(const QJsonObject& object, QString& name)
{
	// Check if "name" key exists:
	auto nameIt = object.find("name");
	if (nameIt == object.end())
	{
		Logger::logit("WARNING: Found no name for object in project file!");
		return false;
	}
	name = nameIt->toString();

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
