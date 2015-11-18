#ifndef PROJECT_H
#define PROJECT_H

#include "common.h"

#include <QStandardItemModel>
#include <QString>
#include <QVariant>

class ObjectItem;

class Project
{
public:
	Project();
	virtual ~Project();

	bool create(); // New
	bool open(const QString& path);
	bool close();
	bool save();
	bool saveAs(const QString& path);

	ObjectItem* addObject(QJsonObject& data);
	QJsonObject removeObject(int id);
	// Update the data of the object with id data["id"] => data MUST contain object id
	ObjectItem* modifyObject(QJsonObject& data);

	QJsonObject getObject(int id);

	bool isEmpty() const { return m_empty; };
	bool hasFilename() const { return !m_path.isEmpty(); };
	const QString& getFilename() const { return m_path; };
	QStandardItemModel& getModel() { return m_objectModel; };


private:
	ObjectItem* findItem(int id);
	bool verifyObject(const QJsonObject& object);

	QString m_path;
	bool m_empty;

	QStandardItemModel m_objectModel;

	static int s_id;

};


#endif