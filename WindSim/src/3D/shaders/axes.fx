#include "common.fx"

cbuffer cb
{
	float4x4 g_mView;
	float4x4 g_mProj;
	float4x4 g_mViewProj;
	float4x4 g_mViewProjInv;
	float4x4 g_mWorldViewProj;
}

struct VSIn
{
	float3 pos : POSITION;
	float3 col : COLOR;
};

struct PSIn
{
	float4 pos : SV_Position;
	float3 col : COLOR;
};

struct PSOut
{
	float4 col : SV_Target;
	float depth : SV_Depth;
};


PSIn vsProject(VSIn vsIn)
{
	PSIn vsOut;
	vsOut.pos = mul(float4(vsIn.pos, 1.0f), g_mWorldViewProj);
	vsOut.col = vsIn.col;
	return vsOut;
}

uint vsPassId(uint vid : SV_VertexID) : VertexID
{
	return vid;
}

static const float3 axesPos[4] = { float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };
static const int2 axesLines[3] = { int2(0, 1), int2(0, 2), int2(0, 3) };

#define ratio 0.7 // letter width/height
static const float3 xPos[4] = { float3(-ratio, 1, 0), float3(ratio, 1, 0), float3(-ratio, -1, 0), float3(ratio, -1, 0) };
static const int2 xLines[2] = { int2(0, 3), int2(1, 2) };

static const float3 yPos[4] = { float3(-ratio, 1, 0), float3(ratio, 1, 0), float3(0, 0, 0), float3(-ratio, -1, 0) };
static const int2 yLines[2] = { int2(0, 2), int2(1, 3) };

static const float3 zPos[4] = { float3(-ratio - 0.1, 1, 0), float3(ratio, 1, 0), float3(-ratio, -1, 0), float3(ratio + 0.2, -1, 0) };
static const int2 zLines[3] = { int2(0, 1), int2(1, 2), int2(2, 3) };



[maxvertexcount(20)]
void gsAxes(point uint input[1] : VertexID, inout LineStream<PSIn> stream)
{
	float3 col[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };

	// Axes
	float4 tmp = mul(float4(-0.2, -0.2, 0.5, 1.0), g_mViewProjInv);
	float3 posWorld = tmp.xyz/tmp.w;

	float4 pos[4];
	for (int i = 0; i < 4; ++i)
		pos[i] = mul(float4(posWorld + axesPos[i] * 30, 1.0), g_mViewProj);

	PSIn v;

	for (int j = 0; j < 3; ++j)
	{
		int2 li = axesLines[j];
		v.col = col[j];

		v.pos = pos[li.x];
		stream.Append(v);
		v.pos = pos[li.y];
		stream.Append(v);

		stream.RestartStrip();
	}

	// Letters

	// x
	float3 xPosView = mul(float4(posWorld + axesPos[1] * 40, 1.0), g_mView).xyz;

	for (i = 0; i < 4; ++i)
		pos[i] = mul(float4(xPosView + xPos[i] * 5, 1.0), g_mProj);

	v.col = col[0];
	for (j = 0; j < 2; ++j)
	{
		int2 li = xLines[j];
		v.pos = pos[li.x];
		stream.Append(v);
		v.pos = pos[li.y];
		stream.Append(v);

		stream.RestartStrip();
	}

	// y
	float3 yPosView = mul(float4(posWorld + axesPos[2] * 40, 1.0), g_mView).xyz;

		for (i = 0; i < 4; ++i)
			pos[i] = mul(float4(yPosView + yPos[i] * 5, 1.0), g_mProj);

	v.col = col[1];
	for (j = 0; j < 2; ++j)
	{
		int2 li = yLines[j];
		v.pos = pos[li.x];
		stream.Append(v);
		v.pos = pos[li.y];
		stream.Append(v);

		stream.RestartStrip();
	}

	// z
	float3 zPosView = mul(float4(posWorld + axesPos[3] * 40, 1.0), g_mView).xyz;

		for (i = 0; i < 4; ++i)
			pos[i] = mul(float4(zPosView + zPos[i] * 4, 1.0), g_mProj);

	v.col = col[2];
	for (j = 0; j < 3; ++j)
	{
		int2 li = zLines[j];
		v.pos = pos[li.x];
		stream.Append(v);
		v.pos = pos[li.y];
		stream.Append(v);

		stream.RestartStrip();
	}
}

float4 psSimple(PSIn psIn) : SV_Target
{
	return float4(psIn.col, 1.0f);
}

PSOut psColOnTop(PSIn psIn)
{
	PSOut p;
	p.col = float4(psIn.col, 1.0f);
	p.depth = 0.0;
	return p;
}


technique11 Simple
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsProject()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psSimple()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}

	pass P1
	{
		SetVertexShader(CompileShader(vs_5_0, vsPassId()));
		SetGeometryShader(CompileShader(gs_5_0, gsAxes()));
		SetPixelShader(CompileShader(ps_5_0, psColOnTop()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}
