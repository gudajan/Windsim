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

	bool addObject(const QString& name, ObjectType type, const QVariant& data = QVariant());

	bool isEmpty() const { return m_empty; };
	bool unsavedChanges() const { return m_unsavedChanges; };
	bool hasFilename() const { return !m_path.isEmpty(); };
	const QString& getFilename() const { return m_path; };
	QStandardItemModel& getModel() { return m_objectModel; };


private:
	QString verifyObject(const QJsonObject& object);
	QString m_path;
	bool m_empty;
	bool m_unsavedChanges;

	QStandardItemModel m_objectModel;

};


#endif