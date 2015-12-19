#include "objectItem.h"
#include "commands.h"

#include <QJsonObject>
#include <QUndoStack>
#include <QApplication>
#include <QPalette>

extern QUndoStack g_undoStack;

ObjectItem::ObjectItem(const QString& text)
	: QStandardItem(text)
{
}

ObjectItem::~ObjectItem()
{
}

void ObjectItem::setData(const QVariant& value, int role)
{
	// If name is changed at the GUI, the name of the internal object must change too
	if (role == Qt::EditRole)
	{
		QJsonObject oldInfo = data().toJsonObject();
		QJsonObject newInfo = oldInfo;
		newInfo.insert("name", value.toString());
		QUndoCommand* modCmd = new ModifyObjectCmd(oldInfo, newInfo, this, Name);
		g_undoStack.push(modCmd);
		return;
	}
	// Set other data as usual
	return QStandardItem::setData(value, role);
}

QVariant ObjectItem::data(int role) const
{
	if (role == Qt::ForegroundRole)
	{
		if (QStandardItem::data().toJsonObject()["disabled"].toInt() != Qt::Unchecked)
		{
			return QVariant(QApplication::palette().color(QPalette::Disabled, QPalette::Foreground));
		}
	}
	return QStandardItem::data(role);
}