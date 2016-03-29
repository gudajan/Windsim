

//enum CellType : char
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

RasterizerState CullNone
{
	CullMode = None;
};

RasterizerState CullBack
{
	CullMode = Back;
};

RasterizerState CullFront
{
	CullMode = Front;
};

DepthStencilState DepthDefault
{
};

DepthStencilState DepthDisable
{
	DEPTHENABLE = FALSE;
};

DepthStencilState DepthWriteDisable
{
	DEPTHENABLE = FALSE;
	DEPTHWRITEMASK = 0;
};

DepthStencilState DepthEqual
{
	DepthFunc = LESS_EQUAL;
};


BlendState BlendDisable
{
};

BlendState BlendBackToFront
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = INV_SRC_ALPHA;
	BlendOp = ADD;

	SrcBlendAlpha = ONE;
	DestBlendAlpha = INV_SRC_ALPHA;
	BlendOpAlpha = ADD;
};

SamplerState SamLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
};

// normal dir, view dir, diffuse color, ambient constant, diffuse constant, specular constant, shininess constant
float4 BlinnPhongIllumination(in float3 n, in float3 v, in float3 col, in float ca, in float cd, in float cs, in int css)
{
	const float3 ambientDiffuseColor = col;
	const float4 lightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	n = normalize(n);
	v = normalize(v);
	const float3 l = v; // Special case: Headlight
	const float3 h = l; // still headlight

	float4 ambient = ca * float4(ambientDiffuseColor, 1.0f);
	float4 diffuse = cd * float4(ambientDiffuseColor, 1.0f) * saturate(dot(n, l));
	float4 specular = cs * pow(saturate(dot(n, h)), css);

	float4 color = ambient + (diffuse + specular) * lightColor;
	color.a = 1.0f;

	return color;
}

bool equal(float a, float b, float epsilon)
{
	return abs(a - b) <= epsilon;
}

bool intersectAlignedBox(in float3 origin, in float3 dir, in float3 boxMin, in float3 boxMax, out float t0, out float t1)
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

	// Avoid intersections behind the ray origin (i.e. if t1 is negative)
	return t0 <= t1 && t1 >= 0.0f; // this has numerical errors if t0 and t1 are almost equal
}