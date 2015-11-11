#include "project.h"

Project::Project()
	: m_path(),
	m_empty(true),
	m_unsavedChanges(false)
{
}

Project::~Project()
{
}

bool Project::create()
{
	//TODO
	m_empty = false;
	return true;
}

bool Project::open(const std::string& path)
{
	//TODO
	m_path = path;
	m_empty = false;
	m_unsavedChanges = false;
	return true;
}
bool Project::close()
{
	//TODO
	m_path.clear();
	m_empty = true;
	m_unsavedChanges = false;
	return true;
}

bool Project::save()
{
	//TODO
	m_unsavedChanges = false;
	return true;
}
bool Project::saveAs(const std::string& path)
{
	//TODO
	m_path = path;
	m_unsavedChanges = false;
	return true;
}