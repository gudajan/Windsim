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
	m_unsavedChanges(false),
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
		const QString& name = verifyObject(obj);
		if (name.isEmpty())
			continue;
		QStandardItem* item = new QStandardItem(name);
		item->setData(obj);
		m_objectModel.appendRow(item);
	}

	m_path = path;
	m_empty = false;
	m_unsavedChanges = false;
	return true;
}
bool Project::close()
{
	m_objectModel.clear();
	m_path.clear();
	m_empty = true;
	m_unsavedChanges = false;
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
	m_unsavedChanges = false;
	return true;
}

// Store objects with their propterties as QJsonObjects
// This way, every object can have custom properties, without creating special object classes
// The objects and its properties also may be saved to file in a human readable and modifiable format
bool Project::addObject(const QString& name, ObjectType type, const QVariant& data)
{
	QJsonObject json
	{
		{ "name", name },
		{ "type", QString::fromStdString(objectTypeToString(type)) }
	};

	if (type == ObjectType::Mesh)
	{
		json.insert("obj-file", data.toString());
	}

	QStandardItem* item = new QStandardItem(name);
	item->setData(json);
	m_objectModel.appendRow(item);

	return true;
}

QString Project::verifyObject(const QJsonObject& object)
{
	// Check if "name" key exists:
	auto nameIt = object.find("name");
	if (nameIt == object.end())
	{
		Logger::logit("WARNING: Found no name for object in project file!");
		return "";
	}
	QString name = nameIt->toString();

	// Check if "type" key exists:
	auto typeIt = object.find("type");
	if (typeIt == object.end())
	{
		Logger::logit("WARNING: Found no type for object '" + name + "' in project file!");
		return "";
	}

	ObjectType type = stringToObjectType(typeIt->toString().toStdString());

	// Verify all mandatory keys for meshes exist
	if (type == ObjectType::Mesh)
	{
		auto objIt = object.find("obj-file");
		if (objIt == object.end())
		{
			Logger::logit("WARNING: Found no obj-file for Mesh '" + name + "' in project file!");
			return "";
		}
	}

	return name;
}
