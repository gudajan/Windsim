#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QJsonObject>

#include "common.h"

class Project;
class DX11Widget;

class AddObjectCmd : public QUndoCommand
{
public:
	AddObjectCmd(QJsonObject info, Project* project, DX11Widget* viewer, QUndoCommand* parent = nullptr);
	~AddObjectCmd();

	void undo() Q_DECL_OVERRIDE;
	void redo() Q_DECL_OVERRIDE;

private:
	QJsonObject m_data;
	Project* m_project;
	DX11Widget* m_viewer;
};

class RemoveObjectCmd : public QUndoCommand
{
public:
	RemoveObjectCmd(QString name, Project* project, DX11Widget* viewer, QUndoCommand* parent = nullptr);
	~RemoveObjectCmd();

	void undo() Q_DECL_OVERRIDE;
	void redo() Q_DECL_OVERRIDE;

private:
	QJsonObject m_data;
	Project* m_project;
	DX11Widget* m_viewer;
};

#endif