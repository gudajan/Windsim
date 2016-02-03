#include "common.fx"

//enum VoxelType : char
//{
//	CELL_TYPE_FLUID = 0,
//	CELL_TYPE_INFLOW, // 1
//	CELL_TYPE_OUTFLOW, // 2
//	CELL_TYPE_SOLID_SLIP, // 3
//	CELL_TYPE_SOLID_NO_SLIP, // 4
//  CELL_TYPE_SOLID_BOUNDARY // 5
//};

#define CELL_TYPE_FLUID 0
#define CELL_TYPE_INFLOW 1
#define CELL_TYPE_OUTFLOW 2
#define CELL_TYPE_SOLID_SLIP 3
#define CELL_TYPE_SOLID_NO_SLIP 4
#define CELL_TYPE_SOLID_BOUNDARY 5

#define XY_PLANE 0
#define XZ_PLANE 1
#define YZ_PLANE 2

// Read-Write and Read texture for the grid, containing a single mesh voxelization
RWTexture3D<uint> g_gridUAV;
Texture3D<uint> g_gridSRV;
// Read-Write and Read texture for the grid, containing all combined voxelizations
RWTexture3D<uint> g_gridAllUAV;
Texture3D<uint> g_gridAllSRV;

Texture3D<float4> g_velocitySRV;



cbuffer cb
{
	float4x4 g_mWorldViewProj; // Object space -> world space -> Camera/View space -> projection space

	float4x4 g_mObjToWorld; // Mesh object space -> world space

	float4x4 g_mGridToVoxel; // Grid Object Space -> Voxel Space (use position as 3D index into voxel grid texture)
	float4x4 g_mWorldToVoxel; // Combined matrix: world space -> grid object space -> voxel space
	float4x4 g_mVoxelProj; // Projection matrix for voxelization (orthogonal, aligned with the voxelgrid)
	float4x4 g_mVoxelWorldViewProj; // Voxel space -> grid object space -> world space -> camera/view space -> projection space
	float4x4 g_mVoxelWorldView; // Voxel space -> grid object space -> world space -> camera/view space

	float4 g_vCamPos; // Camera position (in currently necessary space)
	uint3 g_vResolution; // Resolution of voxel grid
	float3 g_vVoxelSize; // VoxelSize in grid object space

	int g_sGlyphOrientation;
	uint2 g_vGlyphQuantity;
	float g_sGlyphPosition;
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

struct PSColIn
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
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
	return all(pos >= float3(0.0f, 0.0f, 0.0f)) && all(pos < float3(g_vResolution)); // Index out of bounds
}

bool inCellGrid(in int3 pos)
{
	return all(pos >= int3(0, 0, 0)) && all(pos < int3(g_vResolution.x / 4, g_vResolution.yz));
}

uint getVoxelValue(in uint cell, in uint index)
{
	return (cell >> (8 * index)) & 0xff; // shift n-th voxel to front byte
}

uint setVoxelValue(in uint cell, in uint voxel, in uint index)
{
	uint temp = 0xff << 8 * index;
	cell &= ~temp; // Set voxel to zero
	cell |= voxel << 8 * index; // Set voxel value
	return cell;
}

uint getValue(in float3 posVS, out bool posInGrid)
{
	posInGrid = inGrid(posVS);
	if (!posInGrid)
		return CELL_TYPE_FLUID;

	uint x = posVS.x / 4;
	uint n = posVS.x - x * 4;
	uint val = g_gridAllSRV[uint3(x, posVS.yz)];
	return getVoxelValue(val, n);
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

// 4*16*16 = 1024
// As one cell contains 4 voxel in x direction
[numthreads(4, 16, 16)]
void csCombine(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID < uint3(0, 0, 0)) || any(threadID > uint3(g_vResolution.x / 4, g_vResolution.yz))) // Index out of bounds
		return;

	// At this point the voxel only have values CELL_TYPE_FLUID or CELL_TYPE_SOLID_NO_SLIP (0 and 4 respectively)
	// -> it is save to just OR the values
	g_gridAllUAV[threadID] |= g_gridSRV[threadID]; // OR with new value
}

// Von Neumann Neighbourhood
static const int3 neighbours[] = { int3(-1, 0, 0), int3(1, 0, 0), int3(0, -1, 0), int3(0, 1, 0), int3(0, 0, -1), int3(0, 0, 1) };

[numthreads(4, 16, 16)]
void csCellType(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID < uint3(0, 0, 0)) || any(threadID > uint3(g_vResolution.x / 4, g_vResolution.yz))) // Index out of bounds
		return;

	// Get voxel values of current cell
	uint cell = g_gridAllUAV[threadID];
	uint cellVoxel[4];
	for (int v = 0; v < 4; ++v)
	{
		cellVoxel[v] = getVoxelValue(cell, v);
	}

	// Get all necessary neighbour cells
	uint neighbourCells[6];
	for (int i = 0; i < 6; ++i)
	{
		int3 ni = threadID + neighbours[i];
		if (inCellGrid(ni))
		{
			neighbourCells[i] = g_gridAllUAV[ni];
		}

	}

	// Iterate all 4 voxels in current cell
	for (int u = 0; u < 4; ++u)
	{
		int3 posVS = uint3(threadID.x * 4 + u, threadID.yz);

		if (posVS.x == 0)
		{
			cellVoxel[u] = CELL_TYPE_INFLOW;
			continue;
		}
		else if (posVS.x == int(g_vResolution.x) - 1)
		{
			cellVoxel[u] = CELL_TYPE_OUTFLOW;
			continue;
		}

		if (posVS.y == 0 || posVS.y == int(g_vResolution.y) - 1 || posVS.z == 0 || posVS.z == int(g_vResolution.z) - 1)
		{
			cellVoxel[u] = CELL_TYPE_SOLID_SLIP;
			continue;
		}

		if (cellVoxel[u] != CELL_TYPE_SOLID_NO_SLIP)
			continue;

		// Check if a neighbour exists, which contains fluid -> boundary voxel
		// 0 -> -x; 1 -> +x; 2 -> -y; 3 -> +y; 4 -> -z; 5 -> +z;
		for (int i = 0; i < 6; i++)
		{
			// Neighbour outside grid
			if (any(posVS + neighbours[i] < int3(0, 0, 0)) || any(posVS + neighbours[i] >= int3(g_vResolution)))
				continue;

			uint neighbourVoxel;
			if (i == 0)
			{
				if (u <= 0) neighbourVoxel = getVoxelValue(neighbourCells[i], 3); // left neighbour of left voxel in cell
				else neighbourVoxel = cellVoxel[u - 1]; // left voxel neighbour in current cell
			}
			else if (i == 1)
			{
				if (u >= 3) neighbourVoxel = getVoxelValue(neighbourCells[i], 0); // right neighbour of right voxel in cell
				else neighbourVoxel = cellVoxel[u + 1]; // right voxel neighbour in current cell
			}
			else neighbourVoxel = getVoxelValue(neighbourCells[i], u); // y,z neighbours with same index within cell

			if (neighbourVoxel != CELL_TYPE_SOLID_NO_SLIP && neighbourVoxel != CELL_TYPE_SOLID_BOUNDARY)
			{
				cellVoxel[u] = CELL_TYPE_SOLID_BOUNDARY;
				break;
			}
		}
	}

	// Build cell from voxels
	for (int w = 0; w < 4; ++w)
	{
		cell = setVoxelValue(cell, cellVoxel[w], w);
	}

	uint old;
	InterlockedExchange(g_gridAllUAV[threadID], cell, old);
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

uint vsPassId(uint vid : SV_VertexID) : VertexID
{
	return vid;
}

// =============================================================================
// GEOMETRY SHADER
// =============================================================================

[maxvertexcount(5)]
void gsArrowGlyph(point uint input[1] : VertexID, inout LineStream<PSColIn> stream)
{
	PSColIn psIn;

	// Definitions
	float slicePosition = g_sGlyphPosition;
	uint2 glyphNumber = g_vGlyphQuantity;
	int orientation = g_sGlyphOrientation;
	float scale = 0.15 * length(g_vVoxelSize) / length(glyphNumber); // Scaling depends on size of voxel and number of glyphs + magic value
	float arrowHeadSize = 0.5f;

	// Glyph plane orientation
	float3 slicePlaneNormal;
	float3 slicePlaneVec1;
	float3 slicePlaneVec2;
	if (orientation == XZ_PLANE)
	{
		slicePlaneNormal = float3(0, 1, 0);
		slicePlaneVec1 = float3(1, 0, 0);
		slicePlaneVec2 = float3(0, 0, 1);
	}
	else if (orientation == XY_PLANE)
	{
		slicePlaneNormal = float3(0, 0, 1);
		slicePlaneVec1 = float3(1, 0, 0);
		slicePlaneVec2 = float3(0, 1, 0);
	}
	else if (orientation == YZ_PLANE)
	{
		slicePlaneNormal = float3(1, 0, 0);
		slicePlaneVec1 = float3(0, 0, 1);
		slicePlaneVec2 = float3(0, 1, 0);
	}


	float3 slicePlanePos = slicePosition * slicePlaneNormal;

	slicePlaneVec1 /= glyphNumber.x;
	slicePlaneVec2 /= glyphNumber.y;

	uint id = input[0];
	uint2 index;
	index.y = id / glyphNumber.x;
	index.x = id - index.y * glyphNumber.x;

	// Calculate glyph position
	float3 posTS = slicePlanePos + slicePlaneVec1 * (index.x + 0.5) + slicePlaneVec2 * (index.y + 0.5); // In texture space [0, 1]
	float3 posOS = posTS * float3(g_vResolution) * g_vVoxelSize; // In object space

	float3 velocity = g_velocitySRV.SampleLevel(SamLinear, posTS, 0).xyz;

	float3 x = float3(0, -1, 0) * scale;
	float3 y = float3(1, 0, 0) * scale;
	psIn.col = float3(1, 0, 0);
	if (length(velocity) != 0.0)
	{
		// The orientation of the glyph
		x = normalize(velocity) * scale * length(velocity);
		y = normalize(cross(x, normalize((g_vCamPos.xyz * g_vVoxelSize) - posOS))) * scale * length(velocity); // In voxel grid object space
		psIn.col = float3(0, 1, 0);
	}

	//Arrow end
	psIn.pos = mul(float4(posOS - x, 1.0f), g_mWorldViewProj);
	stream.Append(psIn);

	//Arrow head
	psIn.pos = mul(float4(posOS + x, 1.0f), g_mWorldViewProj);
	float4 head = psIn.pos;
	stream.Append(psIn);

	//Arrow head UP
	psIn.pos = mul(float4(posOS + y * arrowHeadSize + x * (1 - arrowHeadSize), 1.0f), g_mWorldViewProj);
	stream.Append(psIn);

	stream.RestartStrip();

	//Arrow head
	psIn.pos = head;
	stream.Append(psIn);

	//Arrow head DOWN
	psIn.pos = mul(float4(posOS - y  * arrowHeadSize + x * (1 - arrowHeadSize), 1.0f), g_mWorldViewProj);
	stream.Append(psIn);
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

	// Calculate real index of current cell, which voxel index of current fragment falls into (one cell is 1 * 1 * 4 as one int contains 4 voxels/chars -> one cell contains 4 voxel in z direction)
	uint currentCell = zRun / 4; // Integer division without rest
	uint n = zRun - currentCell * 4; // = zRun % 4 Voxel index within the current cell

	// Completely xor the cells in front of the fragment
	for (uint z = 0; z < currentCell; ++z)
	{
		// XOR all 4 voxel with CELL_TYPE_SOLID_NO_SLIP = 0x04
		InterlockedXor(g_gridUAV[uint3(z, index.yx)], 0x04040404);
	}

	if (n > 0)
	{
		// Set all voxels in current cell up to index of current fragment to CELL_TYPE_SOLID_NO_SLIP
		uint xorVal = 0x04040404 >> (8 * (4 - n));
		InterlockedXor(g_gridUAV[uint3(currentCell, index.yx)], xorVal);
	}
}

// Ray casting into voxel grid -> render first voxel
PSOut psVolume(PSVoxelIn psIn)
{
	PSOut psOut = (PSOut)0;

	// Default
	float3 voxelColor = float3(0.7f, 0.2f, 0.2f);
	psOut.col = float4(0.0, 0.0f, 0.0f, 1.0f);
	psOut.depth = 0.0f;

	float3 rayDir = normalize(psIn.voxelPos - g_vCamPos.xyz);

	float tmin = 0.0;
	float tmax = -1.0;
	// Add/Subtract small value because of numerical inaccuracies, which would lead to discarded pixels later
	// (i.e. the intersection position may lie just outside the grid and therefore the raycasting is immediately stopped)
	if (!intersectBox(g_vCamPos.xyz, rayDir, float3(0.001f, 0.001f, 0.001f), float3(g_vResolution - 0.001), tmin, tmax))
	{
		discard;
		return psOut;
	}

	float stepSize = 0.1; // In Voxel space, all sides of a voxel are of length 1
	float3 rayInc = stepSize * rayDir;
	float3 currentPos = g_vCamPos.xyz + tmin * rayDir; // Start raycasting at the incoming fragment, which corresponds to the border of the voxel grid
	if (inGrid(g_vCamPos.xyz))
		currentPos = g_vCamPos.xyz;
	bool discarded = false;
	int counter = 0;
	uint voxel = 0;

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
		if (voxel == CELL_TYPE_SOLID_NO_SLIP || voxel == CELL_TYPE_SOLID_BOUNDARY) // Found a voxel
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

	if (voxel == CELL_TYPE_SOLID_NO_SLIP || voxel == CELL_TYPE_SOLID_BOUNDARY)
	{
		// Calculate output
		tmin = 0.0f;
		tmax = -1.0f;
		float3 boxMin = floor(currentPos);
		float3 boxMax = ceil(currentPos);
		intersectBox(g_vCamPos.xyz, rayDir, boxMin, boxMax, tmin, tmax); // Get intersection point of voxel and ray

		float3 voxelPos = g_vCamPos.xyz + tmin * rayDir;
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

		if (voxel == CELL_TYPE_SOLID_BOUNDARY)
			voxelColor = float3(1.0f, 0, 0);
		psOut.col = BlinnPhongIllumination(nor, -viewDir, voxelColor, 0.3f, 0.6f, 0.1f, 4);
	}

	return psOut;
}

float4 psCol(PSColIn frag) : SV_Target
{
	return float4(frag.col, 1.0f);
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
	pass VelocityGlyph
	{
		SetVertexShader(CompileShader(vs_5_0, vsPassId()));
		SetGeometryShader(CompileShader(gs_5_0, gsArrowGlyph()));
		SetPixelShader(CompileShader(ps_5_0, psCol()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass Combine
	{
		SetComputeShader(CompileShader(cs_5_0, csCombine()));
	}
	pass CellType
	{
		SetComputeShader(CompileShader(cs_5_0, csCellType()));
	}
}
