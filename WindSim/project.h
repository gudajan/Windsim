#ifndef PROJECT_H
#define PROJECT_H

#include <string>

class Project
{
public:
	Project();
	virtual ~Project();

	bool create(); // New
	bool open(const std::string& path);
	bool close();
	bool save();
	bool saveAs(const std::string& path);

	bool isEmpty() { return m_empty; };
	bool unsavedChanges() { return m_unsavedChanges; };
	bool hasFilename() { return !m_path.empty(); };

private:
	std::string m_path;
	bool m_empty;
	bool m_unsavedChanges;

};


#endif