#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <limits>

namespace Geometry
{

	// Assuming a vertex in the vertexBuffer has the size vertexSize in bytes and the position consists of 3 floats at the beginning of each vertex
	// TODO perhabs solve with lambda for getting vertex position to allow arbitrary layouts
	static bool intersect(const DirectX::XMFLOAT3&  origin, const DirectX::XMFLOAT3& direction, const std::vector<float>& vb, const std::vector<uint32_t>& ib, int vertexSize, DirectX::XMFLOAT3& intersection)
	{
		using namespace DirectX;

		float distance = std::numeric_limits<float>::infinity();

		const XMVECTOR dir = XMLoadFloat3(&direction);
		const XMVECTOR ori = XMLoadFloat3(&origin);

		// Iterate each triangle
		for (int i = 0; i < ib.size(); i += 3)
		{
			int i0 = ib[i] * vertexSize; // Real index into vertexbuffer
			int i1 = ib[i + 1] * vertexSize;
			int i2 = ib[i + 2] * vertexSize;

			const XMVECTOR p0 = XMVectorSet(vb[i0], vb[i0 + 1], vb[i0 + 2], 1.0f);
			const XMVECTOR p1 = XMVectorSet(vb[i1], vb[i1 + 1], vb[i1 + 2], 1.0f);
			const XMVECTOR p2 = XMVectorSet(vb[i2], vb[i2 + 1], vb[i2 + 2], 1.0f);

			// Intersect triangle
			float curDist = std::numeric_limits<float>::infinity();
			bool intersects = TriangleTests::Intersects(ori, dir, p0, p1, p2, curDist);
			if (intersects)
				distance = curDist;
		}

		// No intersection occured
		if (std::isinf<float>(distance))
			return false;

		XMVECTOR position = ori + distance * dir;
		XMStoreFloat3(&intersection, position);
		return true;
	}
}

#endif