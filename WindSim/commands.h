#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QJsonObject>

#include "common.h"

class ObjectContainer;
class ObjectItem;


enum Modification
{
	Position,
	Scaling,
	Rotation,
	Visibility,
	Shading,
	Name
};

Q_DECLARE_FLAGS(Modifications, Modification);
Q_DECLARE_OPERATORS_FOR_FLAGS(Modifications);

static QString modToString(Modification mod)
{
	switch (mod)
	{
	case(Position) : return "Position";
	case(Scaling) : return "Scaling";
	case(Rotation) : return "Rotation";
	case(Visibility) : return "Visibility";
	case(Name) : return "Name";
	}
	return "";
}


//=================================================================================
class AddObjectCmd : public QUndoCommand
{
public:
	AddObjectCmd(QJsonObject data, ObjectContainer* container, QUndoCommand* parent = nullptr);
	~AddObjectCmd();

	void undo() override;
	void redo() override;

private:
	QJsonObject m_data;
	ObjectContainer* m_container;
};

//=================================================================================
class RemoveObjectCmd : public QUndoCommand
{
public:
	RemoveObjectCmd(int id, ObjectContainer* container, QUndoCommand* parent = nullptr);
	~RemoveObjectCmd();

	void undo() override;
	void redo() override;

private:
	QJsonObject m_data;
	ObjectContainer* m_container;
};

//=================================================================================
class ModifyObjectCmd : public QUndoCommand
{
public:
	ModifyObjectCmd(QJsonObject oldData, QJsonObject newData, ObjectItem* item, Modifications mod, QUndoCommand* parent = nullptr);
	~ModifyObjectCmd();

	void undo() override;
	void redo() override;
	bool mergeWith(const QUndoCommand* cmd) override;

private:
	QJsonObject m_oldData;
	QJsonObject m_newData;
	ObjectItem* m_item;
	Modifications m_mod;
};

#endif