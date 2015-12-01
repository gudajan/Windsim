#ifndef TRANSFORM_MACHINE_H
#define TRANSFORM_MACHINE_H

#include <unordered_map>
#include <vector>
#include <memory>

#include <QJsonObject>
#include <QPoint>

#include "meshActor.h"

namespace State
{
	enum ModificationState { Start, Translate, TranslateX, TranslateY, TranslateZ, Scale, ScaleX, ScaleY, ScaleZ, Rotate, RotateX, RotateY, RotateZ, Aborted, Finished };
}

class ObjectManager;
class Camera;
class QKeyEvent;
class QMouseEvent;

class TransformMachine
{
public:
	TransformMachine(ObjectManager* manager, Camera* camera);
	~TransformMachine();

	void handleKeyPress(QKeyEvent* event, QPoint currentMousePos = QPoint(-1, -1)); // Change states
	void handleMousePress(QMouseEvent* event); // Abort(right)/Finish(left)/Nothing(middelbutton)
	void handleMouseMove(QMouseEvent* event); // Modify selected actors; calculate transformation from current and old cursor position dependent on current view matrix

	std::vector<QJsonObject> getTransformation(); // Return transformed data, reset machine
	void reset() { m_state = State::Start; m_oldActors.clear(); m_transformation.clear(); }; // change to start (reset machine)

	bool isModifying() const { return m_state != State::Start && m_state != State::Aborted && m_state != State::Finished; };
	bool isAborted() const { return m_state == State::Aborted; };

private:
	void start(QPoint currentMousePos); // save selected actors and cursor position
	void abort(); // Restore saved actors; reset machine;
	void finish(); // Save current actor data in QJsonObjects (transfromed data);

	void translate(QPoint currentCursorPos);
	void scale(QPoint move);
	void rotate(QPoint move);

	State::ModificationState m_state;
	std::unordered_map<int, std::shared_ptr<MeshActor>> m_oldActors; // The selected actors at the start of the transformation
	QPoint m_oldCursorPos; // The cursor position at the start of the transformation
	DirectX::XMFLOAT3 m_oldCursorDir; // The vector throug the cursor positon in world space at the start of the transformation
	std::vector<QJsonObject> m_transformation;

	ObjectManager* m_manager;
	Camera* m_camera;
};

#endif