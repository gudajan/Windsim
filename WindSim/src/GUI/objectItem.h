#ifndef OBJECT_ITEM_H
#define OBJECT_ITEM_H

#include <QStandardItem>

class ObjectItem : public QStandardItem
{
public:
	ObjectItem(const QString& text);
	~ObjectItem();

	void setData(const QVariant& value, int role = Qt::UserRole + 1) override;

	QVariant data(int role = Qt::UserRole + 1) const override;

	int inline type() const override { return ItemType::UserType; };
};

#endif