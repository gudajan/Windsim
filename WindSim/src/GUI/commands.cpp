#include "commands.h"
#include "objectContainer.h"
#include "dx11widget.h"
#include "objectItem.h"

#include <QJsonArray>

//=================================================================================
AddObjectCmd::AddObjectCmd(QJsonObject data, ObjectContainer* container, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(data),
	m_container(container)
{
	setText(QObject::tr(qPrintable("Add " + m_data["type"].toString() + " " + m_data["name"].toString())));
}

AddObjectCmd::~AddObjectCmd()
{
}

void AddObjectCmd::redo()
{
	// signal rowsInserted() will be emitted -> object3D added too
	m_container->add(stringToObjectType(m_data["type"].toString().toStdString()), m_data);
}

void AddObjectCmd::undo()
{
	// signal rowsAboutToBeRemoved() will be emitted -> object3D has a chance to remove objects
	m_container->remove(m_data["id"].toInt());
}

//=================================================================================
RemoveObjectCmd::RemoveObjectCmd(int id, ObjectContainer* container, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_data(),
	m_container(container)
{
	m_data = m_container->getData(id);
	setText(QObject::tr(qPrintable("Remove " + m_data["type"].toString() + " " + m_data["name"].toString())));
}

RemoveObjectCmd::~RemoveObjectCmd()
{
}

void RemoveObjectCmd::redo()
{
	m_container->remove(m_data["id"].toInt());
}

void RemoveObjectCmd::undo()
{
	// Add object assigns new unique id
	m_container->add(stringToObjectType(m_data["type"].toString().toStdString()), m_data);
}

//=================================================================================
ModifyObjectCmd::ModifyObjectCmd(QJsonObject oldData, QJsonObject newData, ObjectItem* item, Modifications mod, QUndoCommand* parent)
	: QUndoCommand(parent),
	m_oldData(oldData),
	m_newData(oldData), // Set newData to oldData for the time being, update it later with the newData
	m_item(item),
	m_mod(mod)
{
	// Overwrite the old data with the new data if available
	for (auto it = newData.constBegin(); it != newData.constEnd(); ++it)
	{
		if (it->isDouble()) m_newData[it.key()] = it->toDouble(); // This includes integers
		else if (it->isObject()) m_newData[it.key()] = it->toObject();
		else if (it->isArray()) m_newData[it.key()] = it->toArray();
		else if (it->isString()) m_newData[it.key()] = it->toString();
		else if (it->isBool()) m_newData[it.key()] = it->toBool();
	}

	// Set the type of modification for the data
	m_oldData["modifications"] = static_cast<int>(mod);
	m_newData["modifications"] = static_cast<int>(mod);

	setText(QObject::tr(qPrintable("Modify " + newData["type"].toString() + " " + newData["name"].toString())));
}

ModifyObjectCmd::~ModifyObjectCmd()
{
}

void ModifyObjectCmd::redo()
{
	// This will emit itemChanged once per setData call
	if(m_mod.testFlag(Name))
		m_item->setData(m_newData["name"].toString(), Qt::DisplayRole);

	m_item->setData(m_newData);
}

void ModifyObjectCmd::undo()
{
	// This will emit itemChanged once per setData call
	if (m_mod.testFlag(Name))
		m_item->setData(m_oldData["name"].toString(), Qt::DisplayRole);

	m_item->setData(m_oldData);
}

bool ModifyObjectCmd::mergeWith(const QUndoCommand* cmd)
{
	const ModifyObjectCmd* moc = static_cast<const ModifyObjectCmd*>(cmd);

	if (m_oldData["id"].toInt() != moc->m_oldData["id"].toInt()) return false;

	Modifications m = moc->m_mod;
	QJsonObject json = moc->m_newData;

	// Old data remains the same
	// Update new data parts, which are modified by newer command
	if (m.testFlag(Position)) m_newData["Position"] = json["Position"].toObject();
	if (m.testFlag(Scaling)) m_newData["Scaling"] = json["Scaling"].toObject();
	if (m.testFlag(Rotation)) m_newData["Rotation"] = json["Rotation"].toObject();
	if (m.testFlag(Visibility))
	{
		m_newData["disabled"] = json["disabled"].toInt();
		ObjectType type = stringToObjectType(m_newData["type"].toString().toStdString());
		if (type == ObjectType::VoxelGrid)
			m_newData["renderVoxel"] = json["renderVoxel"].toInt();
		if (type == ObjectType::Mesh)
			m_newData["voxelize"] = json["voxelize"].toInt();
	}
	if (m.testFlag(WindTunnelSettings)) m_newData["windTunnelSettings"] = json["windTunnelSettings"].toString();
	if (m.testFlag(Shading)) m_newData["Shading"] = json["Visibility"].toString();
	if (m.testFlag(Name)) m_newData["name"] = json["name"].toString();
	if (m.testFlag(Color)) m_newData["Color"] = json["Color"].toObject();
	if (m.testFlag(Resolution)) m_newData["resolution"] = json["resolution"].toObject();
	if (m.testFlag(VoxelSize)) m_newData["voxelSize"] = json["voxelSize"].toObject();
	if (m.testFlag(GlyphSettings)) m_newData["glyphs"] = json["glyphs"].toObject();
	if (m.testFlag(VolumeSettings)) m_newData["volume"] = json["volume"].toObject();
	if (m.testFlag(RunSimulation)) m_newData["runSimulation"] = json["runSimulation"].toBool();
	if (m.testFlag(SmokeSettings)) m_newData["smoke"] = json["smoke"].toObject();
	if (m.testFlag(LineSettings)) m_newData["lines"] = json["lines"].toObject();

	return true;
}