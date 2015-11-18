#include "commands.h"
#include "project.h"
#include "dx11widget.h"
#include "objectItem.h"

AddObjectCmd::AddObjectCmd(QJsonObject data, Project* project, DX11Widget* viewer, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(data),
	m_project(project),
	m_viewer(viewer)
{
	setText(QObject::tr(qPrintable("Add " + m_data["type"].toString() + " " + m_data["name"].toString())));
}

AddObjectCmd::~AddObjectCmd()
{
}

void AddObjectCmd::redo()
{
	// addObject adds unique id to the object
	m_project->addObject(m_data); // Called by reference
	m_viewer->addObject3D(m_data);
}

void AddObjectCmd::undo()
{
	m_project->removeObject(m_data["id"].toInt());
	m_viewer->removeObject3D(m_data["id"].toInt());
}

RemoveObjectCmd::RemoveObjectCmd(int id, Project* project, DX11Widget* viewer, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(),
	m_project(project),
	m_viewer(viewer)
{
	m_data = m_project->getObject(id);
	setText(QObject::tr(qPrintable("Remove " + m_data["type"].toString() + " " + m_data["name"].toString())));
}

RemoveObjectCmd::~RemoveObjectCmd()
{
}

void RemoveObjectCmd::redo()
{
	m_project->removeObject(m_data["id"].toInt());
	m_viewer->removeObject3D(m_data["id"].toInt());
}

void RemoveObjectCmd::undo()
{
	// Add object assigns new unique id
	m_project->addObject(m_data);
	m_viewer->addObject3D(m_data);
}