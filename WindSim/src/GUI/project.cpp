#include "project.h"
#include "staticLogger.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QByteArray>


Project::Project()
	: m_path(),
	m_empty(true)
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

bool Project::open(ObjectContainer& container, const QString& path)
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
		StaticLogger::logit("ERROR: No objects present within the project file '" + path + "'!");
		return false;
	}

	if (!it->isArray())
	{
		StaticLogger::logit("ERROR: Objects not saved as Json-Array within the project file '" + path + "'!");
		return false;
	}

	// Iterate all objects in array
	for (auto i : it->toArray())
	{
		QJsonObject obj = i.toObject();
		container.addCmd(obj);
	}

	m_path = path;
	m_empty = false;
	return true;
}
bool Project::close(ObjectContainer& container)
{
	container.clear();
	m_path.clear();
	m_empty = true;
	return true;
}

bool Project::save(ObjectContainer& container)
{
	return saveAs(container, m_path);
}

bool Project::saveAs(ObjectContainer& container, const QString& path)
{
	// Create Json file from all objects
	const std::unordered_set<int> ids = container.getIdList();
	QJsonArray objects;
	for (auto id : ids)
	{
		QJsonObject json = container.getData(id);
		json.remove("id");
		json.remove("modifications");
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


