#ifndef COMMON_H
#define COMMON_H

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <fstream>
#include <cmath>

#include <QMetaType>

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

enum class ObjectType {Invalid, Mesh, Sky, Axes };

Q_DECLARE_METATYPE(ObjectType) // Necessary for qRegisterMetaType() and to pass ObjectType via signals/slots

static inline std::string objectTypeToString(ObjectType type)
{
	switch (type)
	{
	case(ObjectType::Mesh) : return "Mesh";
	case(ObjectType::Sky) : return "Sky";
	case(ObjectType::Axes) : return "Axes";
	}
	return "Invalid";
}

static inline ObjectType stringToObjectType(const std::string& str)
{
	if(str == "Mesh") return ObjectType::Mesh;
	else if (str == "Sky") return ObjectType::Sky;
	else if (str == "Axes") return ObjectType::Axes;
	return ObjectType::Invalid;
}

enum CameraType { FirstPerson, ModelView };

enum class Shading{ Smooth, Flat }; // Mesh Shading type

enum Modification
{
	Position = 0x1,
	Scaling = 0x2,
	Rotation = 0x4,
	Visibility = 0x8,
	Shading = 0x10,
	Name = 0x20,
	Color = 0x40,
	All = UINT_MAX
};

Q_DECLARE_FLAGS(Modifications, Modification);
Q_DECLARE_OPERATORS_FOR_FLAGS(Modifications);

static inline float degToRad(float degree)
{
	return degree / 180.0f * DirectX::XM_PI;
}
static inline float radToDeg(float radians)
{
	return radians / DirectX::XM_PI * 180.0f;
}

// Normalize the given angle in radians to [0°, 360°], respectively returning a value between [0 - 2*PI]
static inline float normalizeRad(float rad)
{
	return rad - (std::floor(rad / (2 * DirectX::XM_PI)) * 2 * DirectX::XM_PI);
}


static void toEuler(DirectX::XMFLOAT3 axis, float angle, float& al, float& be, float& ga) {
	float s = std::sin(angle);
	float c = std::cos(angle);
	float t = 1.0f - c;

	if (DirectX::XMVector3Equal(DirectX::XMVectorZero(), DirectX::XMLoadFloat3(&axis)))
	{
		al = be = ga = 0.0f;
		return;
	}
	assert(DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&axis))) > 1.0f - 1.0e-4f &&
		DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&axis))) < 1.0f + 1.0e-4f);

	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	if ((x * y * t + z  *s) > 0.998f) { // north pole singularity detected
		ga = 2 * std::atan2( x * std::sin( angle / 2.0f ), std::cos( angle / 2.0f )); // yaw
		al = DirectX::XM_PI / 2; // pitch
		be = 0; // roll
		return;
	}
	if ((x * y * t + z * s) < -0.998f) { // south pole singularity detected
		ga = -2.0f * std::atan2(x* std::sin(angle / 2.0f), std::cos(angle / 2.0f));
		al = -DirectX::XM_PI / 2.0f;
		be = 0;
		return;
	}
	ga = std::atan2(y * s - x * z * t, 1.0f - (y * y + z * z) * t);
	al = std::asin(x * y * t + z * s);
	be = std::atan2(x * s - y * z * t, 1.0f - (x * x + z * z) * t);
}

static std::string loadFile(const std::string& path)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	if (in)
	{
		std::string str;
		in.seekg(0, std::ios::end);
		str.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&str[0], str.size());
		in.close();
		return str;
	}
	throw std::runtime_error("Failed to open the file '" + path + "' for reading");
}

static void storeFile(const std::string& path, const std::string& data)
{
	std::ofstream out(path, std::ios::out | std::ios::binary);
	if (out)
	{
		out.write(data.data(), data.size());
		out.close();
	}
	throw std::runtime_error("Failed to open the file '" + path + "' for writing");
}

#endif