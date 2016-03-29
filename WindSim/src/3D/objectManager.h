#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include "object3D.h"
#include "actor.h"
#include "common.h"

#include <unordered_map>
#include <memory>
#include <unordered_set>

#include <QJsonObject>

// Replace: Clear selection and add currently hovered
// Switch: switch selection state of currently hovered
// Clear: Clear selection
enum class Selection {Replace, Switch, Clear};

class ObjectManager
{
public:
	ObjectManager(DX11Renderer* renderer);

	// Add one object, which is rendered
	void add(ID3D11Device* device, const QJsonObject& data);
	// Remove one object
	void remove(int id);
	void removeAll();

	// Trigger function of a single object
	void triggerObjectFunction(const QJsonObject& data);

	void modify(const QJsonObject& data);

	// Render ALL objects at their current transformation
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime);
	// Release the DirectX objects of ALL objects
	void release(bool withAccessories);

	// VoxelGrid access
	void voxelizeNextFrame(); // Issue a voxelization for all voxelgrids in the next frame
	void initOpenCL();
	void runSimulation(bool enabled);

	void updateCursor(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, bool containsCursor); // Update id of hovered object
	bool updateSelection(Selection op);
	void setHovered();
	void setSelected(); // Set currently hovered object to selected if its a mesh; returns if selection changed

	int getHoveredId() const { return m_hoveredId; };
	const std::unordered_set<int>& getSelection(){ return m_selectedIds; };
	const void setSelection(const std::unordered_set<int>& selection);
	std::shared_ptr<Actor> getActor(int id) { return m_actors[id]; }
	std::unordered_map<int, std::shared_ptr<Actor>>& getActors() { return m_actors; };

	void addAccessoryObject(const std::string& name, std::shared_ptr<Object3D> obj, std::shared_ptr<Actor> act);
	std::shared_ptr<Actor> getAccessory(const std::string& name) { return m_accessoryActors[name]; };

private:
	// Get ID of object with intersection
	int computeIntersection(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float& distance) const;

	void log(const std::string& msg);

	int m_hoveredId; // ID of object which is currently hovered by the mouse
	std::unordered_set<int> m_selectedIds; // Contains all ids of objects, that are currently selected

	// Dynamic objects, included in the project
	std::unordered_map<int, std::shared_ptr<Object3D>> m_objects;
	std::unordered_map<int, std::shared_ptr<Actor>> m_actors;

	// Additionally needed objects (e.g. Transform marker, Text displays etc)
	std::unordered_map<std::string, std::shared_ptr<Object3D>> m_accessoryObjects;
	std::unordered_map<std::string, std::shared_ptr<Actor>> m_accessoryActors;

	DX11Renderer* m_renderer;
};

#endif