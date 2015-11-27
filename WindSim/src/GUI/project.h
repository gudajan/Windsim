#ifndef PROJECT_H
#define PROJECT_H

#include "common.h"
#include "objectContainer.h"

#include <QString>

class Project
{
public:
	Project();
	virtual ~Project();

	bool create(); // New
	bool open(ObjectContainer& container, const QString& path);
	bool close(ObjectContainer& container);
	bool save(ObjectContainer& container);
	bool saveAs(ObjectContainer& container, const QString& path);

	bool isEmpty() const { return m_empty; };
	bool hasFilename() const { return !m_path.isEmpty(); };
	const QString& getFilename() const { return m_path; };

private:
	QString m_path;
	bool m_empty;

};


#endif