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
	ObjectManager();
	~ObjectManager();

	// Add one object, which is rendered
	void add(ID3D11Device* device, const QJsonObject& data);
	// Remove one object
	void remove(int id);
	void removeAll();

	void modify(const QJsonObject& data);

	// Render ALL objects at their current transformation
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);
	// Release the DirectX objects of ALL objects
	void release();

	void updateCursor(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, bool containsCursor); // Update id of hovered object
	bool updateSelection(Selection op);
	void setHovered();
	void setSelected(); // Set currently hovered object to selected if its a mesh; returns if selection changed


	int getHoveredId() const { return m_hoveredId; };
	const std::unordered_set<int>& getSelection(){ return m_selectedIds; };
	const void setSelection(const std::unordered_set<int>& selection);
	std::shared_ptr<Actor> getActor(int id) { return m_actors[id]; }
	std::unordered_map<int, std::shared_ptr<Actor>>& getActors() { return m_actors; };

private:
	// Get ID of object with intersection
	int computeIntersection(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float& distance) const;

	int m_hoveredId; // ID of object which is currently hovered by the mouse
	std::unordered_set<int> m_selectedIds; // Contains all ids of objects, that are currently selected

	std::unordered_map<int, std::shared_ptr<Object3D>> m_objects;
	std::unordered_map<int, std::shared_ptr<Actor>> m_actors;
};

#endif