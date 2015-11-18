#include "objectItem.h"

#include <QJsonObject>

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
	if (role == Qt::EditRole || role == Qt::DisplayRole)
	{
		QJsonObject json = QStandardItem::data().toJsonObject();
		QString oldName = json["name"].toString();
		QString name = value.toString();
		json.insert("name", value.toString());
		QStandardItem::setData(json);
	}
	// Set other data as usual
	return QStandardItem::setData(value, role);
}