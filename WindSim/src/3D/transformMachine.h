#ifndef TRANSFORM_MACHINE_H
#define TRANSFORM_MACHINE_H

#include <unordered_map>
#include <vector>
#include <memory>

#include <QJsonObject>
#include <QPoint>

#include <DirectXMath.h>

namespace State
{
	enum ModificationState { Start, Translate, TranslateX, TranslateY, TranslateZ, Scale, ScaleX, ScaleY, ScaleZ, Rotate, RotateX, RotateY, RotateZ, Aborted, Finished };
}

class Actor;
class MarkerActor;
class ObjectManager;
class Camera;
class DX11Renderer;

struct ID3D11Device;

class TransformMachine
{
public:
	TransformMachine(ObjectManager* manager, Camera* camera, DX11Renderer* renderer);

	void initDX11(ID3D11Device* device);

	void handleKeyPress(Qt::Key key, QPoint currentMousePos = QPoint(-1, -1)); // Change states
	void handleMousePress(Qt::MouseButton button); // Abort(right)/Finish(left)/Nothing(middelbutton)
	void handleMouseMove(QPoint localPos, Qt::KeyboardModifiers modifiers); // Modify selected actors; calculate transformation from current and old cursor position dependent on current view matrix

	std::vector<QJsonObject> getTransformation(); // Return transformed data, reset machine
	void reset() { m_state = State::Start; m_oldActors.clear(); m_transformation.clear(); }; // change to start (reset machine)

	bool isModifying() const { return m_state != State::Start && m_state != State::Aborted && m_state != State::Finished; };
	bool isAborted() const { return m_state == State::Aborted; };

private:
	void start(QPoint currentMousePos); // save selected actors calculate all values, which have to be computed only once (cursorPos, averaged object position ...)
	void abort(); // Restore saved actors; reset machine;
	void finish(); // Save current actor data in QJsonObjects (transfromed data);

	void translate(QPoint currentCursorPos, Qt::KeyboardModifiers mods);
	void scale(QPoint currentCursorPos, Qt::KeyboardModifiers mods);
	void rotate(QPoint currentCursorPos, Qt::KeyboardModifiers mods);

	void switchToNone();
	void switchToX();
	void switchToY();
	void switchToZ();

	static void toEuler(DirectX::XMFLOAT4 q, float& al, float& be, float& ga);

	State::ModificationState m_state;
	std::shared_ptr<MarkerActor> m_marker;
	std::unordered_map<int, std::shared_ptr<Actor>> m_oldActors; // The selected actors at the start of the transformation
	DirectX::XMFLOAT3 m_oldObjWorldPos; // The averaged position of all objects in world space
	QPoint m_oldObjWindowPos; // The averaged position of all objects in window/widget space [0 - width/height]
	QPoint m_oldCursorPos; // The cursor position at the start of the transformation
	DirectX::XMFLOAT3 m_oldCursorDir; // The vector throug the cursor positon in world space at the start of the transformation
	DirectX::XMFLOAT3 m_oldXZInt; // The intersection of the original cursor ray and the xz plane
	DirectX::XMFLOAT3 m_oldXYInt; // same but xy plane
	DirectX::XMFLOAT3 m_oldYZInt; // same but yz plane
	std::vector<QJsonObject> m_transformation; // Holds the finished transformation

	ObjectManager* m_manager;
	DX11Renderer* m_renderer;
};

#endif