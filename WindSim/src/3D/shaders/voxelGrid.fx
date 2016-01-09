#include "common.fx"

// Read-Write and Read texture for the grid, containing a single mesh voxelization
RWTexture3D<uint> g_gridUAV;
Texture3D<uint> g_gridSRV;
// Read-Write and Read texture for the grid, containing all combined voxelizations
RWTexture3D<uint> g_gridAllUAV;
Texture3D<uint> g_gridAllSRV;

cbuffer cb
{
	float4x4 g_mWorldViewProj; // Object space -> world space -> Camera/View space -> projection space
	float4x4 g_mObjToWorld; // Mesh object space -> world space
	float4x4 g_mGridToVoxel; // Grid Object Space -> Voxel Space (use position as 3D index into voxel grid texture)
	float4x4 g_mWorldToVoxel; // Combined matrix: world space -> grid object space -> voxel space
	float4x4 g_mVoxelProj; // Projection matrix for voxelization (orthogonal, aligned with the voxelgrid)
	float4x4 g_mVoxelWorldViewProj; // Voxel space -> grid object space -> world space -> camera/view space -> projection space
	float4x4 g_mVoxelWorldViewProjInv; // Inverse of the above
	float4x4 g_mVoxelWorldView; // Voxel space -> grid object space -> world space -> camera/view space

	float4 g_vCamPosVS; // Camera position in Voxel space
	uint3 g_vResolution; // Resolution of voxel grid
	float3 g_vVoxelSize; // VoxelSize in grid object space
}

// =============================================================================
// STRUCTURES
// =============================================================================
struct VSGridIn
{
	float3 pos : POSITION;
};

struct VSMeshIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSGridBoxIn
{
	float4 pos : SV_Position;
};

struct PSVoxelIn
{
	float4 pos : SV_Position;
	float4 projPos : POSITION;
	float3 voxelPos : INDEX;
};

struct PSOut
{
	float4 col : SV_Target;
	float depth : SV_Depth;
};

// =============================================================================
// FUNCTIONS
// =============================================================================

bool inGrid(in float3 pos)
{
	if (any(pos < float3(0.0f, 0.0f, 0.0f)) || any(pos > float3(g_vResolution))) // Index out of bounds
		return false;
	return true;
}

bool getValue(in float3 posVS, out bool posInGrid)
{
	posInGrid = inGrid(posVS);
	if (!posInGrid)
		return false;

	uint x = posVS.x / 32;
	uint n = posVS.x - x * 32;
	uint val = g_gridAllSRV[uint3(x, posVS.yz)];
	// check if nth bit is set
	val &= (1 << n);
	return val > 0;
}

bool intersectBox(in float3 origin, in float3 dir, in float3 boxMin, in float3 boxMax, out float t0, out float t1)
{
	float3 invDir = 1.0f / dir;
	float3 tbot = (boxMin - origin) * invDir;
	float3 ttop = (boxMax - origin) * invDir;

	t0 = (dir.x >= 0) ? tbot.x : ttop.x;
	t1 = (dir.x >= 0) ? ttop.x : tbot.x;

	t0 = max((dir.y >= 0) ? tbot.y : ttop.y, t0);
	t1 = min((dir.y >= 0) ? ttop.y : tbot.y, t1);

	t0 = max((dir.z >= 0) ? tbot.z : ttop.z, t0);
	t1 = min((dir.z >= 0) ? ttop.z : tbot.z, t1);

	return t0 <= t1; // this has numerical errors if t0 and t1 are almost equal
}

// =============================================================================
// COMPUTE SHADER
// =============================================================================

// 1*32*32 = 1024
// As one cell contains 32 voxel in x direction
[numthreads(1, 32, 32)]
void csCombine(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID < uint3(0, 0, 0)) || any(threadID > uint3(g_vResolution.x / 32, g_vResolution.yz))) // Index out of bounds
		return;

	g_gridAllUAV[threadID] |= g_gridSRV[threadID]; // Or with new value
}

// =============================================================================
// VERTEX SHADER
// =============================================================================
PSGridBoxIn vsGridBox(VSGridIn vsIn)
{
	PSGridBoxIn vsOut;
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	return vsOut;
}

PSVoxelIn vsVoxelize(VSMeshIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(mul(float4(vsIn.pos, 1.0f), g_mObjToWorld), g_mWorldToVoxel).xyz; // Transform from mesh object space -> World Space -> grid object space -> grid voxel space
	vsOut.pos = mul(mul(mul(float4(vsIn.pos, 1.0f), g_mObjToWorld), g_mWorldToVoxel), g_mVoxelProj);

	// Shift z coordinate about half a voxel so the voxel position appears to be in its center instead of the lower, left, front corner
	vsOut.voxelPos.z += 0.5;

	// Clamp z coordinate to grid
	// This way, a fragment is generated although it lies outside the grid
	// As the voxelization algorithm depends on the odd number of fragments (entering and leaving the object) it is important to also generate the leaving fragment if it lies outside the grid
	vsOut.pos.z = clamp(vsOut.pos.z, - 1.0f, 1.0f);
	vsOut.projPos = vsOut.pos;
	return vsOut;
}

PSVoxelIn vsVolume(VSGridIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(float4(vsIn.pos, 1.0f), g_mGridToVoxel).xyz; // Transfrom from grid object space -> grid voxel space (aka texture space)
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	vsOut.projPos = vsOut.pos;
	return vsOut;
}

// =============================================================================
// PIXEL SHADER
// =============================================================================
float4 psGridBox(PSGridBoxIn psIn) : SV_Target
{
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

void psVoxelize(PSVoxelIn psIn)
{
	// Voxelization performed along x-axis
	// -> Switch x and z on grid access

	uint3 index = uint3(psIn.voxelPos);
	uint zRun = g_vResolution.x;
	if (psIn.voxelPos.z < 0)
		zRun = 0;
	if (inGrid(float3(psIn.voxelPos.zyx)))
		zRun = index.z;

	// Calculate real index of current cell, which voxel index of current fragment falls into (one cell is 1 * 1 * 32 as one int contains 32 bit/bools -> one cell contains 32 voxel in z direction)
	uint currentCell = zRun / 32; // Integer division without rest
	uint n = zRun - currentCell * 32; // = zRun % 32 Voxel index within the current cell

	// Completely xor the cells in front of the fragment
	for (uint z = 0; z < currentCell; ++z)
	{
		InterlockedXor(g_gridUAV[uint3(z, index.yx)], 0xFFFFFFFF); // Use UINT_MAX
	}

	// Xor all bits in current cell up to index of current fragment
	// For this, set all necessary bits in a temp value to 1 and xor the whole cell with it once
	uint xorVal = 1 << n; // nth bit is set
	xorVal -= 1; // all bits 0..n-1 are set as desired
	InterlockedXor(g_gridUAV[uint3(currentCell, index.yx)], xorVal);
}

// Ray casting into voxel grid -> render first voxel
PSOut psVolume(PSVoxelIn psIn)
{
	PSOut psOut = (PSOut)0;

	// Default
	float3 voxelColor = float3(0.7f, 0.2f, 0.2f);
	psOut.col = float4(0.0, 0.0f, 0.0f, 1.0f);
	psOut.depth = 0.0f;

	float3 rayDir = normalize(psIn.voxelPos - g_vCamPosVS.xyz);

	float tmin = 0.0;
	float tmax = -1.0;
	// Add/Subtract small value because of numerical inaccuracies, which would lead to discarded pixels later
	// (i.e. the intersection position may lie just outside the grid and therefore the raycasting is immediately stopped)
	if (!intersectBox(g_vCamPosVS.xyz, rayDir, float3(0.0001f, 0.0001f, 0.0001f), float3(g_vResolution - 0.0001), tmin, tmax))
	{
		discard;
		return psOut;
	}

	float stepSize = 0.1; // In Voxel space, all sides of a voxel are of length 1
	float3 rayInc = stepSize * rayDir;
	float3 currentPos = g_vCamPosVS.xyz + tmin * rayDir; // Start raycasting at the incoming fragment, which corresponds to the border of the voxel grid
	if (inGrid(g_vCamPosVS.xyz))
		currentPos = g_vCamPosVS.xyz;
	bool discarded = false;
	int counter = 0;
	bool voxel = false;

	int maxSteps = ceil(length(g_vResolution) / stepSize);

	while (counter < maxSteps)
	{
		counter++;

		bool posInGrid = false;
		voxel = getValue(currentPos, posInGrid);
		if (!posInGrid)
		{
			discarded = true;
			break;
		}
		if (voxel) // Found a voxel
		{
			break;
		}
		currentPos += rayInc; // Next step
	}

	// discard statement outside of while loop to avoid compiler problems
	if (discarded)
	{
		discard;
	}

	if (voxel)
	{
		// Calculate output
		tmin = 0.0f;
		tmax = -1.0f;
		float3 boxMin = floor(currentPos);
		float3 boxMax = ceil(currentPos);
		intersectBox(g_vCamPosVS.xyz, rayDir, boxMin, boxMax, tmin, tmax); // Get intersection point of voxel and ray

		float3 voxelPos = g_vCamPosVS.xyz + tmin * rayDir;
		float4 pos = mul(float4(voxelPos, 1.0f), g_mVoxelWorldViewProj);
		psOut.depth = pos.z / pos.w;

		// Calculate normal at intersection point
		float3 nor = float3(0.0f, 0.0f, 0.0f);
		float epsilon = 0.02f;

		if (equal(voxelPos.x, boxMin.x, epsilon))
			nor.x = -1.0f; // left
		else if (equal(voxelPos.x, boxMax.x, epsilon))
			nor.x = 1.0f; // right

		if (equal(voxelPos.y, boxMin.y, epsilon))
			nor.y = -1.0f; // bottom
		else if (equal(voxelPos.y, boxMax.y, epsilon))
			nor.y = 1.0f; // top

		if (equal(voxelPos.z, boxMin.z, epsilon))
			nor.z = -1.0f; // near
		else if (equal(voxelPos.z, boxMax.z, epsilon))
			nor.z = 1.0f; // far

		// Transform into view space
		nor = mul(float4(nor, 0.0f), g_mVoxelWorldView).xyz;
		float3 viewDir = mul(float4(rayDir, 0.0f), g_mVoxelWorldView).xyz;

		psOut.col = BlinnPhongIllumination(nor, -viewDir, voxelColor, 0.3f, 0.6f, 0.1f, 4);
	}

	return psOut;
}


technique11 Voxel
{
	pass Voxelize
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelize()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psVoxelize()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass RenderGridBox
	{
		SetVertexShader(CompileShader(vs_5_0, vsGridBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psGridBox()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass RenderVoxel
	{
		SetVertexShader(CompileShader(vs_5_0, vsVolume()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psVolume()));
		SetRasterizerState(CullFront);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass Combine
	{
		SetComputeShader(CompileShader(cs_5_0, csCombine()));
	}
}
