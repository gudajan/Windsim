#include "commands.h"
#include "project.h"
#include "dx11widget.h"
#include "objectItem.h"

//=================================================================================
AddObjectCmd::AddObjectCmd(QJsonObject data, Project* project, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(data),
	m_project(project)
{
	setText(QObject::tr(qPrintable("Add " + m_data["type"].toString() + " " + m_data["name"].toString())));
}

AddObjectCmd::~AddObjectCmd()
{
}

void AddObjectCmd::redo()
{
	// signal rowsInserted() will be emitted -> object3D added too
	m_project->addObject(m_data);
}

void AddObjectCmd::undo()
{
	// signal rowsAboutToBeRemoved() will be emitted -> object3D has a chance to remove objects
	m_project->removeObject(m_data["id"].toInt());
}

//=================================================================================
RemoveObjectCmd::RemoveObjectCmd(int id, Project* project, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(),
	m_project(project)
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
}

void RemoveObjectCmd::undo()
{
	// Add object assigns new unique id
	m_project->addObject(m_data);
}

//=================================================================================
ModifyObjectCmd::ModifyObjectCmd(QJsonObject oldInfo, QJsonObject newInfo, ObjectItem* item, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_oldData(oldInfo),
	m_newData(newInfo),
	m_item(item)
{
}

ModifyObjectCmd::~ModifyObjectCmd()
{
}

void ModifyObjectCmd::redo()
{
	// This will emit itemChanged
	m_item->setData(m_newData["name"].toString(), Qt::DisplayRole);
	m_item->setData(m_newData);
}

void ModifyObjectCmd::undo()
{
	// This will emit itemChanged
	m_item->setData(m_oldData["name"].toString(), Qt::DisplayRole);
	m_item->setData(m_oldData);
}