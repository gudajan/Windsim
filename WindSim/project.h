#ifndef PROJECT_H
#define PROJECT_H

#include "common.h"

#include <QStandardItemModel>
#include <QString>
#include <QVariant>

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

	bool addObject(const QJsonObject& data);
	QJsonObject removeObject(const QString& name);

	QJsonObject getObject(const QString& name);

	bool isEmpty() const { return m_empty; };
	bool hasFilename() const { return !m_path.isEmpty(); };
	const QString& getFilename() const { return m_path; };
	QStandardItemModel& getModel() { return m_objectModel; };


private:
	QStandardItem* findItem(const QString& name);
	bool verifyObject(const QJsonObject& object, QString& name);

	QString m_path;
	bool m_empty;

	QStandardItemModel m_objectModel;

};


#endif