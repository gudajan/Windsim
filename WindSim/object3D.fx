RasterizerState CullBack
{
	CullMode = Back;
};

DepthStencilState DepthDefault
{
};

BlendState BlendDisable
{
};

cbuffer cb
{
	float4x4 g_worldView;
	float4x4 g_worldViewIT;
	float4x4 g_worldViewProj;
}

struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct PSIn
{
	float4 pos : SV_Position;
	float3 posView : POSITION;
	float3 normalView : NORMAL;
};


PSIn vsProject(VSIn inVertex)
{
	PSIn outVertex;
	outVertex.pos = mul(float4(inVertex.pos, 1.0f), g_worldViewProj);
	outVertex.posView = mul(float4(inVertex.pos, 1.0f), g_worldView).xyz;
	outVertex.normalView = mul(float4(inVertex.normal, 0.0f), g_worldViewIT).xyz;
	return outVertex;
}

float4 psBlinnPhong(PSIn inFragment) : SV_Target
{
	const float ka = 0.2f; // ambient
	const float kd = 0.7f; // diffuse
	const float ks = 0.1f; // specular
	const float s = 1.0f; // shininess (alpha)

	const float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	float3 normal = normalize(cross(ddx(inFragment.posView), ddy(inFragment.posView)));
	// this will all get inlined anyway, might as well make it pretty
	const float3 n = normal;
	const float3 v = -normalize(inFragment.posView);
	const float3 l = v; // special case: headlight
	const float3 h = l; // still headlight

	return color * (ka + kd * saturate(dot(n, l))) + ks * pow(saturate(dot(n, h)), s);
	//return float4((inFragment.normalView * 0.5 + 0.5, 1.0f);
	//return float4(normal * 0.5 + 0.5, 1.0f);
}


technique11 Simple
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsProject()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psBlinnPhong()));
		SetRasterizerState(CullBack);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendDisable, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}