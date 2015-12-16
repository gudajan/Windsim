#ifndef OBJECT_CONTAINER_H
#define OBJECT_CONTAINER_H

#include "objectItem.h"
#include "common.h"
#include "commands.h"
#include "..\3D\dx11renderer.h"

#include <QObject>
#include <QPointer>
#include <QStandardItemModel>
#include <QJsonObject>
#include <unordered_set>

class MeshProperties;
class VoxelGridProperties;

// Contains the object model, which contains the gui objects (e.g. GUI representation of meshes with their own properties)
class ObjectContainer : public QObject
{
	Q_OBJECT

	friend class AddObjectCmd;
	friend class RemoveObjectCmd;
	friend class ModifyObjectCmd;

public:
	ObjectContainer(QWidget* parent = nullptr);
	~ObjectContainer();


	void clear();

	QJsonObject getData(int id);
	ObjectItem* getItem(int id);

	const std::unordered_set<int> getIdList() const { return m_ids; };
	QStandardItemModel& getModel() { return m_model; };
	void setRenderer(DX11Renderer* renderer);

public slots:
	void showPropertiesDialog(const QModelIndex& index);

	void addCmd(const QJsonObject& data);
	void removeCmd(int id);
	// Update the data of the object with id data["id"] => data MUST contain object id
	void modifyCmd(const QJsonObject& data, Modifications mod); // Properties Dialogs and Renderer connect to this
	void rendererModification(std::vector<QJsonObject> data); // Create modify command for each QJsonObject and combine them in group

private slots:
	// Propagate object changes in Object Model to other Views (i.e. 3D Representation and dialogs)
	void objectsInserted(const QModelIndex & parent, int first, int last);
	void objectsRemoved(const QModelIndex & parent, int first, int last);
	void objectModified(QStandardItem * item);

signals:
	void updatePropertiesDialog(const QJsonObject& json); // Propagate changes to the dialogs
	void objectPropertiesChanged(const QJsonObject& json); // Propagate changes to 3D view

	void addObject3DTriggered(const QJsonObject& data);
	void modifyObject3DTriggered(const QJsonObject& data);
	void removeObject3DTriggered(int id);
	void removeAllObject3DTriggered();

private:
	bool verifyData(QJsonObject& object);
	bool add(ObjectType type, QJsonObject& data);
	bool remove(int id);
	bool modify(QJsonObject& data);

	QPointer<MeshProperties> m_meshProperties;
	QPointer<VoxelGridProperties> m_voxelGridProperties;

	std::unordered_set<int> m_ids;
	QStandardItemModel m_model; // Contains the object items
	QPointer<DX11Renderer> m_renderer;

	static int s_id;
};
#endif