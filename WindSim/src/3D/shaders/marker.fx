#include "common.fx"

cbuffer cb
{
	float4x4 g_mWorldView;
	float4x4 g_mProjection;
	float4x4 g_mWorldViewProj;
	bool g_bRenderX;
	bool g_bRenderY;
	bool g_bRenderZ;
	bool g_bRenderLarge;
}

struct VSIn
{
	float3 pos : POSITION;
};

struct GSIn
{
	float3 viewPos : VIEWPOSITION;
};

struct PSSphereIn
{
	float4 pos : SV_POSITION;
	float3 viewPos : VIEWPOSITION;
	float3 center : CENTER;
};

struct PSLineIn
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

struct PSOut
{
	float4 Color : SV_Target;
	float Depth : SV_Depth;
};

// view direction , center of sphere, radius, intersection point, surface normal
bool computeRaySphereIntersection(in float3 D, in float3 C, in float radius, inout float3 X, inout float3 normal)
{
	//Coefficients of Quadratic Equation
	float a = dot(D, D);
	float b = -dot(D, C);
	float c = dot(C, C) - radius * radius;

	//discriminant
	float d = b * b - a * c;

	if (d < 0)
	{
		return false;
	}
	else
	{
		d = sqrt(d);

		// minimal t is always given by (-b -D) / 2a, as scalar a is always positive
		float t = (-b - d) / a;
		float3 X2 = t * D;

		if (length(X2) <= length(X))
		{
			normal = normalize(X2 - C);
			X = X2;
			return true;
		}
		else
		{
			return false;
		}
	}
}

PSOut computeOutput(in float3 X, in float3 D, in float3 normal, in float3 color)
{
	PSOut output;

	output.Color = BlinnPhongIllumination(normal, -D, color, 0.02, 0.88, 0.1, 30);

	float4 clipSpacePos = mul(float4(X, 1.0f), g_mProjection);
	clipSpacePos.z /= clipSpacePos.w;
	output.Depth = clipSpacePos.z;

	return output;
}

GSIn vs(VSIn v)
{
	GSIn output = (GSIn)0;

	output.viewPos = mul(float4(v.pos, 1.0), g_mWorldView).xyz;
	return output;
}

// Creates a quad facing the camera, which is used to calculate a sphere
[maxvertexcount(4)]
void gsSphere(point GSIn p[1], inout TriangleStream<PSSphereIn> stream)
{
	GSIn pos = p[0];

	PSSphereIn vertex;

	float3 upTemp = float3(0.0f, 1.0f, 0.0f);
	float3 lookAt = normalize(pos.viewPos);
	float3 right = normalize(cross(lookAt, upTemp));
	float3 up = normalize(cross(right, lookAt));

	float rad = pos.viewPos.z * 0.01;

	// Left up
	vertex.viewPos = pos.viewPos + rad * up - rad * right - rad * lookAt;
	vertex.pos = mul(float4(vertex.viewPos, 1.0f), g_mProjection);
	vertex.center = pos.viewPos;
	stream.Append(vertex);

	// Right up
	vertex.viewPos = pos.viewPos + rad * up + rad * right - rad * lookAt;
	vertex.pos = mul(float4(vertex.viewPos, 1.0f), g_mProjection);
	vertex.center = pos.viewPos;
	stream.Append(vertex);

	// Left down
	vertex.viewPos = pos.viewPos - rad * up - rad * right - rad * lookAt;
	vertex.pos = mul(float4(vertex.viewPos, 1.0f), g_mProjection);
	vertex.center = pos.viewPos;
	stream.Append(vertex);

	// Right down
	vertex.viewPos = pos.viewPos - rad * up + rad * right - rad * lookAt;
	vertex.pos = mul(float4(vertex.viewPos, 1.0f), g_mProjection);
	vertex.center = pos.viewPos;
	stream.Append(vertex);
}


[maxvertexcount(6)]
void gsLine(point GSIn p[1], inout LineStream<PSLineIn> stream)
{
	GSIn pos = p[0];

	PSLineIn vertex = (PSLineIn)0;

	float size = g_bRenderLarge ? 100000.0f : 1.0f;

	if (g_bRenderX)
	{
		float3 x = float3(1.0f, 0.0f, 0.0f);
		x = mul(float4(x, 0.0f), g_mWorldView).xyz;

		vertex.col = float3(0.5f, 0.0f, 0.0f);
		vertex.pos = mul(float4(pos.viewPos - size * x, 1.0f), g_mProjection);
		stream.Append(vertex);
		vertex.pos = mul(float4(pos.viewPos + size * x, 1.0f), g_mProjection);
		stream.Append(vertex);
		stream.RestartStrip();
	}

	if (g_bRenderY)
	{
		float3 y = float3(0.0f, 1.0f, 0.0f);
		y = mul(float4(y, 0.0f), g_mWorldView).xyz;

		vertex.col = float3(0.0f, 0.5f, 0.0f);
		vertex.pos = mul(float4(pos.viewPos - size * y, 1.0f), g_mProjection);
		stream.Append(vertex);
		vertex.pos = mul(float4(pos.viewPos + size * y, 1.0f), g_mProjection);
		stream.Append(vertex);
		stream.RestartStrip();
	}

	if (g_bRenderZ)
	{
		float3 z = float3(0.0f, 0.0f, 1.0f);
		z = mul(float4(z, 0.0f), g_mWorldView).xyz;

		vertex.col = float3(0.0f, 0.0f, 0.5f);
		vertex.pos = mul(float4(pos.viewPos - size * z, 1.0f), g_mProjection);
		stream.Append(vertex);
		vertex.pos = mul(float4(pos.viewPos + size * z, 1.0f), g_mProjection);
		stream.Append(vertex);
		stream.RestartStrip();
	}
}

PSOut psSphereColor(PSSphereIn fragment)
{
	float3 D = normalize(fragment.viewPos);
	float3 X = float3(0, 0, 100000);
	float3 normal = float3(0, 0, 0);

	PSOut output = (PSOut)0;
	output.Depth = 0;

	float rad = fragment.viewPos.z  * 0.01;

	if (computeRaySphereIntersection(D, fragment.center, rad, X, normal))
	{
		output = computeOutput(X, D, normal, float3(1.0f, 0.6f, 0.3f));
		output.Depth = 0.0f;
		return output;
	}
	else
	{
		discard;
		return output;
	}

	//output.Color = float4(1.0, 0.8, 0.8, 1.0f);
	//return output;
}

PSOut psLineColor(PSLineIn fragment)
{
	PSOut output = (PSOut)0;

	output.Color = float4(fragment.col, 0.5f);
	output.Depth = 0.0f;
	return output;
}

technique11 RenderMarker
{
	pass Position
	{
		SetVertexShader(CompileShader(vs_5_0, vs()));
		SetGeometryShader(CompileShader(gs_5_0, gsSphere()));
		SetPixelShader(CompileShader(ps_5_0, psSphereColor()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}

	pass Axes
	{
		SetVertexShader(CompileShader(vs_5_0, vs()));
		SetGeometryShader(CompileShader(gs_5_0, gsLine()));
		SetPixelShader(CompileShader(ps_5_0, psLineColor()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendBackToFront, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}