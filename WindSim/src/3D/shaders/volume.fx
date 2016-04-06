#include "common.fx"

RWTexture3D<float> g_scalarGridUAV;
Texture3D<float> g_scalarGridSRV;
Texture3D<float4> g_vectorGrid;
Texture2D<float> g_depthTex;
Texture1D<float4> g_txfnTex;

cbuffer cb
{
	// Matrices
	float4x4 g_mProj;
	float4x4 g_mProjInv;
	float4x4 g_mWorldView;
	float4x4 g_mWorldViewProjInv;
	float4x4 g_mTexToGrid;
	float4x4 g_mTexToGridInv; // Grid object space -> voxel space [0, dimensions]^3 -> texture space [0, 1]^3

	// Vectors
	uint3 g_vDimensions;
	float3 g_vVoxelSize;
	float3 g_vCamPosOS; // Camera position in grid object space

	// Scalars
	float g_sRangeMin;
	float g_sRangeMax;
	float g_sStepSize;
};

// =============================================================================
// STRUCTURES
// =============================================================================
struct PSIn
{
	float4 pos : SV_POSITION; // Position in homogenous coordinates
	float3 posOS : POSITION; // Position in object space (of grid)
};

// =============================================================================
// FUNCTIONS
// =============================================================================
bool inGrid(in float3 pos)
{
	return all(pos >= float3(0.0f, 0.0f, 0.0f)) && all(pos < float3(g_vDimensions)); // Index out of bounds
}

float3x3 computeJacobian(in uint3 id)
{
	float3x3 J;

	uint3 neighbours[3];
	neighbours[0] = uint3(1, 0, 0);
	neighbours[1] = uint3(0, 1, 0);
	neighbours[2] = uint3(0, 0, 1);

	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		// Border cases
		float2 fac = float2(id[i] <= 0 ? 0 : 1,
			id[i] >= (g_vDimensions[i] - 1) ? 0 : 1);

		float d = fac.x + fac.y;

		uint3 neighbour = neighbours[i];

		J[0][i] = (g_vectorGrid[id + neighbour * fac.y].x - g_vectorGrid[id - neighbour * fac.x].x) / (d);
		J[1][i] = (g_vectorGrid[id + neighbour * fac.y].y - g_vectorGrid[id - neighbour * fac.x].y) / (d);
		J[2][i] = (g_vectorGrid[id + neighbour * fac.y].z - g_vectorGrid[id - neighbour * fac.x].z) / (d);
	}

	return J;
}

// Jacobi Eigenvalue Algorithm
// Efficient implementation from Book "Mathematics for 3D Game Programming and Computer Graphics" (page 487)
// Skip computation of eigenvectors
float computeSecondLargestEigenvalue(in float3x3 m)
{
	float m11 = m._11;
	float m12 = m._12;
	float m13 = m._13;
	float m22 = m._22;
	float m23 = m._23;
	float m33 = m._33;

	const int maxSweeps = 32;
	const float epsilon = 1.0e-10f;

	for (int a = 0; a < maxSweeps; ++a)
	{
		// Exit if off-diagonal entries small enough.
		if ((abs(m12) < epsilon) && (abs(m13) < epsilon) && (abs(m23) < epsilon))
			break;

		// Annihilate (1,2) entry.
		if (m12 != 0.0f)
		{
			float u = (m22 - m11) * 0.5f / m12;
			float u2 = u * u;
			float u2p1 = u2 + 1.0f;
			float t = (u2p1 != u2) ? ((u < 0.0) ? -1.0f : 1.0f) * (sqrt(u2p1) - abs(u)) : 0.5f / u;
			float c = 1.0f / sqrt(t * t + 1.0f);
			float s = c * t;

			m11 -= t * m12;
			m22 += t * m12;
			m12 = 0.0f;

			float temp = c * m13 - s * m23;
			m23 = s * m13 + c * m23;
			m13 = temp;
		}

		// Annihilate (1,3) entry.
		if (m13 != 0.0f)
		{
			float u = (m33 - m11) * 0.5f / m13;
			float u2 = u * u;
			float u2p1 = u2 + 1.0f;
			float t = (u2p1 != u2) ? ((u < 0.0f) ? -1.0f : 1.0f) * (sqrt(u2p1) - abs(u)) : 0.5f / u;
			float c = 1.0f / sqrt(t * t + 1.0f);
			float s = c * t;

			m11 -= t * m13;
			m33 += t * m13;
			m13 = 0.0f;

			float temp = c * m12 - s * m23;
			m23 = s * m12 + c * m23;
			m12 = temp;
		}

		// Annihilate (2,3) entry.
		if (m23 != 0.0f)
		{
			float u = (m33 - m22) * 0.5f / m23;
			float u2 = u * u;
			float u2p1 = u2 + 1.0f;
			float t = (u2p1 != u2) ? ((u < 0.0f) ? -1.0f : 1.0f) * (sqrt(u2p1) - abs(u)) : 0.5f / u;
			float c = 1.0f / sqrt(t * t + 1.0f);
			float s = c * t;

			m22 -= t * m23;
			m33 += t * m23;
			m23 = 0.0f;

			float temp = c * m12 - s * m13;
			m13 = s * m12 + c * m13;
			m12 = temp;
		}
	}

	// Return second largest eigenvalue
	int second;
	if (m11 < m22)
	{
		if (m22 < m33) second = m22;
		else second = m11 < m33 ? m33 : m11;
	}
	else
	{
		if (m11 < m33) second = m11;
		else second = m22 < m33 ? m33 : m22;
	}
	return second;
}

// Compute trace of a 3x3 matrix
float trace(in float3x3 m)
{
	return m._11 + m._22 + m._33;
}

// Get depth in view space from depth buffer
float sampleDepth(in float4 posSS)
{
	// depth value in NDC from depth texture
	float depthNDC = g_depthTex[posSS.xy];
	// Transform depth from NDC to view space
	float4 depthView = mul(float4(0, 0, depthNDC, 1), g_mProjInv);

	// Dehomogenize
	return depthView.z / depthView.w;
}

float pseudoRand(in float2 uv)
{
	float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233)*2.0)) * 43758.5453));
	return abs(noise.x + noise.y) * 0.5;
}

uint3 clampEdges(uint3 id)
{
	uint3 ret = id;

	int i = 0;
	if (id.x < 3 || id.x > g_vDimensions.x - 3) i++;
	if (id.y < 3 || id.y > g_vDimensions.y - 3) i++;
	if (id.z < 3 || id.z > g_vDimensions.z - 3) i++;

	if (i >= 2)
		ret = clamp(id, uint3(3, 3, 3), uint3(g_vDimensions - 3));

	return ret;
}

// =============================================================================
// COMPUTE SHADER
// =============================================================================
[numthreads(16, 8, 8)]
void csMagnitude(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	uint3 id = clampEdges(threadID);

	g_scalarGridUAV[threadID] = length(g_vectorGrid[id]);
}

[numthreads(16, 8, 8)]
void csVorticity(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	// Avoid drawing artifacts, which arise in the corners and edges of the voxel grid
	uint3 id = clampEdges(threadID);

	float3x3 J = computeJacobian(id);

	float3 curl = float3(J._32 - J._23, J._13 - J._31, J._21 - J._12);

	g_scalarGridUAV[threadID] = length(curl);
}

[numthreads(16, 8, 8)]
void csDivergence(uint3 threadID: SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	uint3 id = clampEdges(threadID);

	float3x3 J = computeJacobian(id);

	float div = J._11 + J._22 + J._33;

	g_scalarGridUAV[threadID] = div;
}

[numthreads(16, 8, 8)]
void csQCriterion(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	uint3 id = clampEdges(threadID);

	float3x3 J = computeJacobian(id);

	float Q = 0.5 * (pow(trace(J), 2) - trace(mul(J, J)));

	g_scalarGridUAV[threadID] = Q;
}

[numthreads(16, 8, 8)]
void csDeltaCriterion(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	uint3 id = clampEdges(threadID);

	float3x3 J = computeJacobian(id);

	float Q = 0.5 * (pow(trace(J), 2) - trace(mul(J, J)));

	float R = -determinant(J);

	float delta = pow(Q / 3.0f, 3) + pow(R * 0.5, 2);

	g_scalarGridUAV[threadID] = delta;
}

[numthreads(16, 8, 8)]
void csLambda2Criterion(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	uint3 id = clampEdges(threadID);

	float3x3 J = computeJacobian(id);

	float3x3 S = 0.5 * (J + transpose(J)); // Orthogonal complement -> symmetric matrix
	float3x3 omega = 0.5 * (J - transpose(J)); // Orthogonal projection -> skew symmetric matrix

	float lambda2 = computeSecondLargestEigenvalue(mul(S, S) + mul(omega, omega));

	g_scalarGridUAV[threadID] = lambda2;
}

// =============================================================================
// VERTEX SHADER
// =============================================================================
PSIn vsScreenTri(uint vid : SV_VertexID)
{
	PSIn v;
	float x = float((vid & 2) << 1) - 1.0;
	float y = 1.0 - float((vid & 1) << 2);
	v.pos = float4 (x, y, 0, 1);

	// Compute "position" in texture space of volume
	float4 gridPos = mul(v.pos, g_mWorldViewProjInv);
	// Dehomogenize
	v.posOS = gridPos.xyz / gridPos.w;
	return v;
}

// =============================================================================
// PIXEL SHADER
// =============================================================================
// Ray casting in grid object space with simple alpha blending
float4 psDirect(PSIn frag) : SV_Target
{
	float stepSize = g_sStepSize * max(g_vVoxelSize.x, max(g_vVoxelSize.y, g_vVoxelSize.z));
	float ocExp = g_sStepSize / 10.0; // Exponent for opacity correction; Make alpha correction quite large, so the transfer function is editable in a more user friendly way

	float3 rayDir = normalize(frag.posOS - g_vCamPosOS); // In texture space

	float tmin = 0.0;
	float tmax = -1.0;

	if (!intersectAlignedBox(frag.posOS, rayDir, float3(0.0f, 0.0f, 0.0f), g_vVoxelSize * g_vDimensions, tmin, tmax))
	{
		discard;
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float numLev;
	float2 dims;
	// get dimension of depth texture (screen width and height)
	g_depthTex.GetDimensions(0.0, dims.x, dims.y, numLev);

	float maxDepthVS = sampleDepth(frag.pos);

	// Increments
	float3 rayInc = stepSize * rayDir;
	float depthVSInc = mul(float4(rayInc, 0), g_mWorldView).z;

	// Start variables
	float depthOS = (tmin > 0.0f ? tmin : 0.0f); // In object space
	float3 rayPos = frag.posOS + rayDir * depthOS; // +pseudoRand(frag.pos.xy / dims) * rayInc;
	float depthVS = mul(float4(rayPos, 1), g_mWorldView).z; // grid space -> world space -> view space

	float4 accCol = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float range = (g_sRangeMax - g_sRangeMin);
	while (accCol.a < 0.99f && depthOS <= tmax && depthVS <= maxDepthVS)
	{
		float scalar = g_scalarGridSRV.SampleLevel(SamLinear, mul(float4(rayPos, 1.0f), g_mTexToGridInv).xyz, 0.0);

		float val = saturate((scalar - g_sRangeMin) / range);

		float4 col = g_txfnTex.SampleLevel(SamLinear, val, 0.0);

		// Opacity correction
		col.a = saturate(1.0 - pow(1.0 - col.a, ocExp));

		// Alpha blending
		accCol.rgb = saturate(accCol.rgb + (1 - accCol.a) * col.rgb * col.a);
		accCol.a = saturate(accCol.a + (1 - accCol.a) * col.a);



		rayPos += rayInc;
		depthVS += depthVSInc;
		depthOS += stepSize;
	}

	if (accCol.a == 0)
	{
		discard;
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	return accCol;
}


technique11 Metric
{
	pass Magnitude
	{
		SetComputeShader(CompileShader(cs_5_0, csMagnitude()));
	}

	pass Vorticity
	{
		SetComputeShader(CompileShader(cs_5_0, csVorticity()));
	}

	pass Divergence
	{
		SetComputeShader(CompileShader(cs_5_0, csDivergence()));
	}

	pass QCriterion
	{
		SetComputeShader(CompileShader(cs_5_0, csQCriterion()));
	}

	pass DeltaCriterion
	{
		SetComputeShader(CompileShader(cs_5_0, csDeltaCriterion()));
	}

	pass Lambda2Criterion
	{
		SetComputeShader(CompileShader(cs_5_0, csLambda2Criterion()));
	}
}

technique11 Render
{
	pass Direct
	{
		SetVertexShader(CompileShader(vs_5_0, vsScreenTri()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psDirect()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthWriteDisable, 0);
		SetBlendState(BlendBackToFront, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}