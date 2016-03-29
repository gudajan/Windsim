#include "common.fx"

RWTexture3D<float> g_scalarGridUAV;
Texture3D<float> g_scalarGridSRV;
Texture3D<float4> g_vectorGrid;
Texture2D<float> g_depthTex;

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
	float3 g_vCamPosTS; // Camera position in texture space [0, 1]^3
};

// =============================================================================
// STRUCTURES
// =============================================================================
struct PSIn
{
	float4 pos : SV_POSITION; // Position in homogenous coordinates
	float3 uvw : TEXCOORD; // Position in texture coordinates
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

// =============================================================================
// COMPUTE SHADER
// =============================================================================
[numthreads(16, 8, 8)]
void csMagnitude(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	g_scalarGridUAV[threadID] = length(g_vectorGrid[threadID]);
}

[numthreads(16, 8, 8)]
void csVorticity(uint3 threadID : SV_DispatchThreadID)
{
	if (!inGrid(threadID))
		return;

	float3x3 J = computeJacobian(threadID);

	float3 curl = float3(J._32 - J._23, J._13 - J._31, J._21 - J._12);

	g_scalarGridUAV[threadID] = length(curl);
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
	float4 texCoord = mul(mul(v.pos, g_mWorldViewProjInv), g_mTexToGridInv);
	// Dehomogenize
	v.uvw = texCoord.xyz / texCoord.w;
	return v;
}

// =============================================================================
// PIXEL SHADER
// =============================================================================
float4 psDirect(PSIn frag) : SV_Target
{
	float stepSize = 0.5f / max(g_vDimensions.x, max(g_vDimensions.y, g_vDimensions.z));

	float maxScalar = 1000.0f;
	float minScalar = 100.0f;

	float3 color = float3(0.4, 0.7, 0.3);

	float3 rayDir = normalize(frag.uvw - g_vCamPosTS); // In texture space

	float tmin = 0.0;
	float tmax = -1.0;

	// Add/Subtract small value because of numerical inaccuracies, which would lead to discarded pixels later
	// (i.e. the intersection position may lie just outside the grid and therefore the raycasting is immediately stopped)
	if (!intersectAlignedBox(frag.uvw, rayDir, float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f), tmin, tmax))
	{
		discard;
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	//return float4(0.1, 0.0, 0.0, tmax - tmin);

	float maxViewDepth = sampleDepth(frag.pos);

	// Start variables
	float texDepth = (tmin > 0.0f ? tmin : 0.0f);
	float3 rayPos = frag.uvw + rayDir * texDepth;
	float viewDepth = mul(mul(float4(rayPos, 1), g_mTexToGrid), g_mWorldView).z; // tex space -> grid space -> world space -> view space

	// Increments
	float3 rayInc = stepSize * rayDir;
	float viewDepthInc = mul(mul(float4(rayInc, 0), g_mTexToGrid), g_mWorldView).z;

	float4 accCol = float4(0.0f, 0.0f, 0.0f, 0.0f);
	int i = 0;
	while (accCol.a < 0.99f && texDepth <= tmax && viewDepth <= maxViewDepth && i < 1024)
	{
		float scalar = g_scalarGridSRV.SampleLevel(SamLinear, rayPos, 0.0);

		float val = saturate((scalar - minScalar) / (maxScalar - minScalar));

		float4 col = float4(color, val);

		// Alpha blending
		accCol.rgb = saturate(accCol.rgb + (1 - accCol.a) * col.rgb * col.a);
		accCol.a = saturate(accCol.a + (1 - accCol.a) * col.a);

		rayPos += rayInc;
		viewDepth += viewDepthInc;
		texDepth += stepSize;
		i++;
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