#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstdint>

namespace objLoader
{
	// Helper to implement all necessary Vector Functions
	struct Vec3
	{
		float x;
		float y;
		float z;

		static Vec3 crossProduct(const Vec3& v1, const Vec3& v2)
		{
			return{ v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x };
		}

		static float dotProduct(const Vec3& v1, const Vec3& v2)
		{
			return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
		}
		float length() const
		{
			return std::sqrtf(x * x + y * y + z * z);
		}
		float lengthSquared() const
		{
			return x*x + y*y + z*z;
		}
		void normalize()
		{
			float l = length();
			x /= l;
			y /= l;
			z /= l;
		}

		Vec3 normalized()
		{
			float l = length();
			return{ x / l, y / l, z / l };
		}

		bool operator==(const Vec3& v) const
		{
			return x == v.x && y == v.y && z == v.z;
		}

		Vec3 operator-(const Vec3& v) const
		{
			return{ x - v.x, y - v.y, z - v.z };
		}
		Vec3 operator+(const Vec3& v) const
		{
			return{ x + v.x, y + v.y, z + v.z };
		}

		Vec3& operator-=(const Vec3& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return *this;
		}
		Vec3& operator+=(const Vec3& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return *this;
		}

		// float * Vec3
		friend Vec3 operator*(const float f, const Vec3& v)
		{
			return{ f*v.x, f*v.y, f * v.z };
		}

		// Vec3 * float
		Vec3 operator*(const float f) const
		{
			return{ x*f, y * f, z * f };
		}

		// Vec3 / float
		Vec3 operator/(const float f) const
		{
			return{ x / f, y / f, z / f };
		}
	};

	struct Vertex
	{
		Vec3 p;
		Vec3 n;

		const bool operator==(const Vertex &v) const
		{
			return p == v.p && n == v.n;
		}
	};
}

class ObjLoader
{
public:
	// Load obj from file <path>
	// Return vertex data in <vertexData>, where one vertex consists of 3 (position) + 3 (normal) = 6 floats
	// Return index data in <indexData>, where one triangle constists of 3 uint32_t
	// Layout is of the vertex data: px, py, pz, nx, ny, nz
	static bool loadObj(const std::string path, std::vector<float>& vertexData, std::vector<uint32_t>& indexData);

	// Normalize the given object, so it fits into a unit Box
	static float normalizeSize(std::vector<float>& vertexData);

	// Calculate the normals from the given data
	// Overwrites every normal in the given data
	// Calculate smoothed normals
	static void calculateNormals(std::vector<float>& vertexData, const std::vector<uint32_t>& indexData);

	// Find the center and radius of a bounding sphere for the given data
	// bsCenter will contain 3 floats representing the x, y and z coordinate.
	static void findBoundingSphere(std::vector<float>& vertexData, std::vector<float>* bsCenter, float* bsRadius);

	static void findBoundingBox(std::vector<float>& vertexData, std::vector<float>& center, std::vector<float>& extends);

private:
	static void addVertexToBuffer(std::vector<float>& b, const objLoader::Vertex& v);
	static objLoader::Vec3 findGeometricCenter(const std::vector<objLoader::Vertex * const>& b);
	static void convertVertexData(std::vector<float>& in, std::vector<objLoader::Vertex * const>* out);
};

namespace  std
{
	template <> struct hash<objLoader::Vertex>
	{
	public:
		uint32_t operator()(const objLoader::Vertex & v) const
		{
			uint32_t h1 = hash<float>()(v.p.x);
			uint32_t h2 = hash<float>()(v.p.y);
			uint32_t h3 = hash<float>()(v.p.z);
			uint32_t h4 = hash<float>()(v.n.x);
			uint32_t h5 = hash<float>()(v.n.y);
			uint32_t h6 = hash<float>()(v.n.z);
			return h1 ^ ((h2 ^ ((h3 ^ ((h4 ^ ((h5 ^ (h6 << 1)) << 1)) << 1)) << 1)) << 1);
		}
	};
}

#endif