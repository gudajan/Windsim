#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QJsonObject>

#include "common.h"

class Project;
class ObjectItem;


//=================================================================================
class AddObjectCmd : public QUndoCommand
{
public:
	AddObjectCmd(QJsonObject data, Project* project, QUndoCommand* parent = nullptr);
	~AddObjectCmd();

	void undo() override;
	void redo() override;

private:
	QJsonObject m_data;
	Project* m_project;
};

//=================================================================================
class RemoveObjectCmd : public QUndoCommand
{
public:
	RemoveObjectCmd(int id, Project* project, QUndoCommand* parent = nullptr);
	~RemoveObjectCmd();

	void undo() override;
	void redo() override;

private:
	QJsonObject m_data;
	Project* m_project;
};

//=================================================================================
class ModifyObjectCmd : public QUndoCommand
{
public:
	ModifyObjectCmd(QJsonObject oldData, QJsonObject newData, ObjectItem* item, QUndoCommand* parent = nullptr);
	~ModifyObjectCmd();

	void undo() override;
	void redo() override;

private:
	QJsonObject m_oldData;
	QJsonObject m_newData;
	ObjectItem* m_item;
};

#endif