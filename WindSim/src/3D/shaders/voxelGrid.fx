#include "common.fx"

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

struct PSVoxelIn
{
	float4 pos : SV_Position;
	float3 voxelPos : INDEX;
};

struct PSColIn
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

struct PSCubeIn
{
	float4 pos : SV_POSITION;
	float3 posView : POSITION;
	nointerpolation uint type : VOXELTYPE;
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

// Get voxel value from cell value cell
uint getVoxelValue(in uint cell, in uint index)
{
	return (cell >> (8 * index)) & 0xff; // shift n-th voxel to front byte
}

// Set voxel value within cell value cell
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

uint3 coordFromIndex(uint index)
{
	uint3 coord;
	coord.z = index / (g_vResolution.x * g_vResolution.y);
	coord.y = (index - coord.z * g_vResolution.x * g_vResolution.y) / g_vResolution.x;
	coord.x = index - g_vResolution.x * (coord.y + coord.z * g_vResolution.y);

	return coord;
}

// =============================================================================
// COMPUTE SHADER
// =============================================================================

// 4*16*16 = 1024
// As one cell contains 4 voxel in x direction
[numthreads(4, 16, 16)]
void csCombine(uint3 threadID : SV_DispatchThreadID)
{
	if (!inCellGrid(threadID)) // Index out of bounds
		return;

	uint cell = g_gridSRV[threadID];
	// At this point the voxel only have values CELL_TYPE_FLUID or CELL_TYPE_SOLID_NO_SLIP (0 and 4 respectively)
	// -> it is save to just OR the values
	g_gridAllUAV[threadID] |= cell; // OR with new value
}

// Von Neumann Neighbourhood
static const int3 neighbours[] = { int3(-1, 0, 0), int3(1, 0, 0), int3(0, -1, 0), int3(0, 1, 0), int3(0, 0, -1), int3(0, 0, 1) };

[numthreads(4, 16, 16)]
void csCellTypeGridBoundary(uint3 threadID : SV_DispatchThreadID)
{
	if (!inCellGrid(threadID)) // Index out of bounds
		return;

	// Get voxel values of current cell
	uint cell = g_gridAllUAV[threadID];

	int3 maxID = int3(g_vResolution)-1;
	int3 minID = int3(0, 0, 0);
	uint cellPosX = threadID.x * 4;

	for (int i = 0; i < 4; ++i)
	{
		uint cellVoxel = getVoxelValue(cell, i);
		int3 posVS = uint3(cellPosX + i, threadID.yz);

		if (posVS.x == 0)
		{
			cellVoxel = CELL_TYPE_INFLOW;
		}
		else if (posVS.x == maxID.x)
		{
			cellVoxel = CELL_TYPE_OUTFLOW;
		}

		if (any(posVS.yz == maxID.yz) || any(posVS.yz == minID.yz))
		{
			cellVoxel = CELL_TYPE_SOLID_SLIP;
		}

		cell = setVoxelValue(cell, cellVoxel, i);
	}

	InterlockedExchange(g_gridAllUAV[threadID], cell, cell);
}

[numthreads(4, 16, 16)]
void csCellTypeSolidBoundary(uint3 threadID : SV_DispatchThreadID)
{
	if (!inCellGrid(threadID)) // Index out of bounds
		return;

	// Get voxel values of current cell
	uint cell = g_gridAllUAV[threadID];
	uint cellVoxel[4];
	int i;
	for (i = 0; i < 4; ++i)
	{
		cellVoxel[i] = getVoxelValue(cell, i);
	}

	// Get all necessary neighbour cells
	uint neighbourCells[6];
	for (i = 0; i < 6; ++i)
	{
		int3 ni = threadID + neighbours[i];
		if (inCellGrid(ni))
		{
			neighbourCells[i] = g_gridAllUAV[ni];
		}
	}

	int3 maxID = int3(g_vResolution)-1;
	int3 minID = int3(0, 0, 0);
	uint cellPosX = threadID.x * 4;

	// Iterate all 4 voxels in current cell
	for (int u = 0; u < 4; ++u)
	{
		int3 posVS = uint3(cellPosX + u, threadID.yz);

		if (cellVoxel[u] != CELL_TYPE_SOLID_NO_SLIP)
			continue;

		// Check if a neighbour exists, which contains fluid -> boundary voxel
		// 0 -> -x; 1 -> +x; 2 -> -y; 3 -> +y; 4 -> -z; 5 -> +z;
		for (i = 0; i < 6; i++)
		{
			// Neighbour outside grid
			int3 npos = posVS + neighbours[i];
			if (any(npos < minID) || any(npos >= int3(g_vResolution)))
			{
				continue;
			}

			uint neighbourVoxel = CELL_TYPE_FLUID;
			if (i == 0)
			{
				if (u == 0)
					neighbourVoxel = getVoxelValue(neighbourCells[0], 3); // left neighbour of left voxel in cell
				else if (u == 1)
					neighbourVoxel = cellVoxel[0]; // left voxel neighbour in current cell
				else if (u == 2)
					neighbourVoxel = cellVoxel[1];
				else if (u == 3)
					neighbourVoxel = cellVoxel[2];
			}
			else if (i == 1)
			{
				if (u == 3)
					neighbourVoxel = getVoxelValue(neighbourCells[1], 0); // right neighbour of right voxel in cell
				else if (u == 2)
					neighbourVoxel = cellVoxel[3]; // right voxel neighbour in current cell
				else if (u == 1)
					neighbourVoxel = cellVoxel[2];
				else if (u == 0)
					neighbourVoxel = cellVoxel[1];
			}
			else
				neighbourVoxel = getVoxelValue(neighbourCells[i], u); // y,z neighbours with same index within cell

			if (neighbourVoxel != CELL_TYPE_SOLID_NO_SLIP && neighbourVoxel != CELL_TYPE_SOLID_BOUNDARY)
			{
				cellVoxel[u] = CELL_TYPE_SOLID_BOUNDARY;
				break;
			}
		}

		cell = setVoxelValue(cell, cellVoxel[u], u);
	}

	InterlockedExchange(g_gridAllUAV[threadID], cell, cell);
}

// =============================================================================
// VERTEX SHADER
// =============================================================================
PSColIn vsGridBox(VSGridIn vsIn)
{
	PSColIn vsOut;
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	vsOut.col = float3(0.0f, 0.0f, 0.0f);
	return vsOut;
}

PSVoxelIn vsVoxelize(VSMeshIn vsIn)
{
	PSVoxelIn vsOut;
	float4 voxelPos = mul(mul(float4(vsIn.pos, 1.0f), g_mObjToWorld), g_mWorldToVoxel); // Transform from mesh object space -> World Space -> grid object space -> grid voxel space
	voxelPos.x = clamp(voxelPos.x, 0, g_vResolution.x); // Clamp mesh to grid extents

	vsOut.voxelPos = voxelPos.xyz;
	vsOut.pos = mul(voxelPos, g_mVoxelProj); // Rotate voxelgrid and apply projection to voxelize along x-axis

	return vsOut;
}

PSVoxelIn vsVolume(VSGridIn vsIn)
{
	PSVoxelIn vsOut;
	vsOut.voxelPos = mul(float4(vsIn.pos, 1.0f), g_mGridToVoxel).xyz; // Transfrom from grid object space -> grid voxel space (aka texture space)
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
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
	//float scale = 0.15 * length(g_vVoxelSize) / length(glyphNumber); // Scaling depends on size of voxel and number of glyphs + magic value
	float scale = 0.012 / length(glyphNumber); // Use Magic number for glyph scaling so glyph size is comparable for different settings
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

	float3 velocity = g_velocitySRV.SampleLevel(SamLinear, posTS, 0.0).xyz;

	float3 x = float3(0, -1, 0) * 0.01f;
	float3 y = float3(1, 0, 0) * 0.01f;
	psIn.col = float3(1, 0, 0);
	if (length(velocity) != 0.0)
	{
		// The orientation of the glyph
		x = normalize(velocity) * scale * length(velocity);
		y = normalize(cross(x, normalize((g_vCamPos.xyz * g_vVoxelSize) - posOS))) * scale * length(velocity); // In voxel grid object space
		psIn.col = float3(0, 0.2, 0.5);
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

static const int3 boxPos[8] =
{
	int3 (0, 0, 0),
	int3 (1, 0, 0),
	int3 (0, 1, 0),
	int3 (1, 1, 0),
	int3 (0, 0, 1),
	int3 (1, 0, 1),
	int3 (0, 1, 1),
	int3 (1, 1, 1)
};
#define BEVEL -1.5
static const float3 boxPosBevel[8] =
{
	float3 (0 + BEVEL, 0 + BEVEL, 0 + BEVEL),
	float3 (1 - BEVEL, 0 + BEVEL, 0 + BEVEL),
	float3 (0 + BEVEL, 1 - BEVEL, 0 + BEVEL),
	float3 (1 - BEVEL, 1 - BEVEL, 0 + BEVEL),
	float3 (0 + BEVEL, 0 + BEVEL, 1 - BEVEL),
	float3 (1 - BEVEL, 0 + BEVEL, 1 - BEVEL),
	float3 (0 + BEVEL, 1 - BEVEL, 1 - BEVEL),
	float3 (1 - BEVEL, 1 - BEVEL, 1 - BEVEL)
};
static const int2 boxLines[12] =
{
	int2(0, 1), int2(0, 2), int2(0, 4), int2(1, 3), int2(1, 5), int2(2, 3), int2(2, 6), int2(3, 7), int2(4, 5), int2(4, 6), int2(5, 7), int2(6, 7)
};
static const int3 boxTris[12] =
{
	int3(0, 3, 1), int3(0, 2, 3), int3(0, 4, 2), int3(0, 1, 5), int3(0, 5, 4), int3(1, 3, 7), int3(1, 7, 5), int3(2, 4, 6), int3(2, 7, 3), int3(2, 6, 7), int3(4, 5, 6), int3(5, 7, 6)
};

[maxvertexcount(36)]
void gsSolidCube(point uint input[1] : VertexID, inout TriangleStream<PSCubeIn> stream)
{
	uint index = input[0];

	uint3 gridPos = coordFromIndex(index);

	uint3 cellId = uint3(gridPos.x / 4, gridPos.yz);
	uint n = gridPos.x - cellId.x * 4;

	uint voxel = getVoxelValue(g_gridAllSRV[cellId], n);

	if (voxel != CELL_TYPE_SOLID_NO_SLIP)
		return;

	float4 pos[8];
	float3 posView[8];
	for (int i = 0; i < 8; ++i)
	{
		pos[i] = mul(float4 ((gridPos + boxPos[i]), 1.0f), g_mVoxelWorldViewProj);
		posView[i] = mul(float4((gridPos + boxPosBevel[i]), 1.0f), g_mVoxelWorldView).xyz;
	}

	PSCubeIn v = (PSCubeIn)0;
	v.type = voxel;

	for (int j = 0; j < 12; ++j)
	{
		int3 tri = boxTris[j];

		v.pos = pos[tri.x];
		v.posView = posView[tri.x];
		stream.Append(v);
		v.pos = pos[tri.y];
		v.posView = posView[tri.y];
		stream.Append(v);
		v.pos = pos[tri.z];
		v.posView = posView[tri.z];
		stream.Append(v);

		stream.RestartStrip();
	}
}

[maxvertexcount(24)]
void gsWireCube(point uint input[1] : VertexID, inout LineStream<PSCubeIn> stream)
{
	uint index = input[0];

	uint3 gridPos = coordFromIndex(index);

	uint3 cellId = uint3(gridPos.x / 4, gridPos.yz);
		uint n = gridPos.x - cellId.x * 4;

	uint voxel = getVoxelValue(g_gridAllSRV[cellId], n);

	if (voxel != CELL_TYPE_SOLID_NO_SLIP)
		return;

	float4 pos[8];
	for (int i = 0; i < 8; ++i)
	{
		pos[i] = mul(float4 ((gridPos + boxPos[i]), 1.0f), g_mVoxelWorldViewProj);
	}

	PSCubeIn v = (PSCubeIn)0;
	v.type = voxel;

	for (int j = 0; j < 12; ++j)
	{
		int2 li = boxLines[j];

		v.pos = pos[li.x];
		stream.Append(v);
		v.pos = pos[li.y];
		stream.Append(v);

		stream.RestartStrip();
	}
}

// =============================================================================
// PIXEL SHADER
// =============================================================================
void psVoxelize(PSVoxelIn psIn)
{
	// Voxelization performed along x-axis
	// -> Switch x and z on grid access

	uint3 index = uint3(psIn.voxelPos);
	uint xRun = g_vResolution.x;
	if (psIn.voxelPos.x < 0)
		xRun = 0;
	else if (psIn.voxelPos.x < int(g_vResolution.x) - 1)
		xRun = index.x;

	// Calculate real index of current cell, which voxel index of current fragment falls into (one cell is 1 * 1 * 4 as one int contains 4 voxels/chars -> one cell contains 4 voxel in z direction)
	uint currentCell = xRun / 4; // Integer division without rest
	uint n = xRun - currentCell * 4; // = xRun % 4 Voxel index within the current cell

	// Completely xor the cells in front of the fragment
	for (uint x = 0; x < currentCell; ++x)
	{
		// XOR all 4 voxel with CELL_TYPE_SOLID_NO_SLIP = 0x04
		InterlockedXor(g_gridUAV[uint3(x, index.yz)], 0x04040404);
	}

	if (n > 0)
	{
		// Set all voxels in current cell up to index of current fragment to CELL_TYPE_SOLID_NO_SLIP
		uint xorVal = 0x04040404 >> (8 * (4 - n));
		InterlockedXor(g_gridUAV[uint3(currentCell, index.yz)], xorVal);
	}

	//return float4(float(xRun.x) / g_vResolution.x, 0, 0, 1);
}

void psConservative(PSVoxelIn psIn)
{
	if (!inGrid(psIn.voxelPos))
		return;

	uint3 index = uint3(psIn.voxelPos); // Adjust for the half voxel shift

	uint cellId = index.x / 4;
	uint n = index.x - cellId * 4;

	// Set voxel with index n within cell to CELL_TYPE_SOLID_NO_SLIP
	uint orVal = 0x04 << 8 * n;
	InterlockedOr(g_gridUAV[uint3(cellId, index.yz)], orVal);
}

float4 psSolidCube(PSCubeIn frag) : SV_Target
{
	float3 col = float3(0.7f, 0.2f, 0.2f);

	float3 n = normalize(cross(ddx_coarse(frag.posView), ddy_coarse(frag.posView)));

	if (frag.type == CELL_TYPE_SOLID_BOUNDARY)
		col = float3(0.8f, 0.1f, 0.0f);

	return BlinnPhongIllumination(n, -normalize(frag.posView), col, 0.3f, 0.6f, 0.1f, 4);
	//return BlinnPhongIllumination(n, normalize(float3(1, 1, -1)), col, 0.3f, 0.6f, 0.1f, 4);
}

float4 psCol(PSColIn frag) : SV_Target
{
	return float4(frag.col, 1.0f);
}

float4 psWireCube(PSCubeIn frag) : SV_Target
{
	float3 col = float3(0.2f, 0.2f, 0.2f);

	if (frag.type == CELL_TYPE_SOLID_BOUNDARY)
		col = float3(0.4f, 0.4f, 0.4f);

	return float4(col, 1.0f);
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

	pass Conservative
	{
		SetVertexShader(CompileShader(vs_5_0, vsVoxelize()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psConservative()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}

	pass RenderGridBox
	{
		SetVertexShader(CompileShader(vs_5_0, vsGridBox()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psCol()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}

	pass SolidVoxel
	{
		SetVertexShader(CompileShader(vs_5_0, vsPassId()));
		SetGeometryShader(CompileShader(gs_5_0, gsSolidCube()));
		SetPixelShader(CompileShader(ps_5_0, psSolidCube()));
		SetRasterizerState(CullBack);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}

	pass WireframeVoxel
	{
		SetVertexShader(CompileShader(vs_5_0, vsPassId()));
		SetGeometryShader(CompileShader(gs_5_0, gsWireCube()));
		SetPixelShader(CompileShader(ps_5_0, psWireCube()));
		SetRasterizerState(CullNone);
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

	pass CellTypeGridBoundary
	{
		SetComputeShader(CompileShader(cs_5_0, csCellTypeGridBoundary()));
	}
	pass CellTypeSolidBoundary
	{
		SetComputeShader(CompileShader(cs_5_0, csCellTypeSolidBoundary()));
	}
}
