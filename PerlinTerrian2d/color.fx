
struct Material {
	float4 ambient;
	float4 diffuse; // w = alpha
	float4 specular; // w = specular power
};

struct DirectionalLight {
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float3 direction;
	float pad;
};

struct PointLight {
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float3 pos;
	float pad;
};

void CalculateColorCausedByDirectionalLight(Material mtl, float3 PosW, float3 EyeW, float3 NormalW, DirectionalLight dLight, out float4 finalColor) {
	finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 litVec = normalize(dLight.direction);

	float4 ambient = mtl.ambient * dLight.ambient;

	float4 diffuse = max(dot(-litVec, NormalW), 0.0f) * mtl.diffuse * dLight.diffuse;

	float3 posToEye = normalize(EyeW - PosW);

	float4 specular = pow(max(dot(reflect(litVec, NormalW), posToEye), 0.0f), mtl.specular.a) * mtl.specular * dLight.specular;

	finalColor = ambient + diffuse + specular;
}

void CalculateColorCausedByPointLight(Material mtl, float3 PosW, float3 EyeW, float3 NormalW, PointLight pLight, out float4 finalColor) {
	finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 firstLitVec = PosW - pLight.pos;
	float3 litVec = normalize(firstLitVec);

		float4 ambient = mtl.ambient * pLight.ambient;

		float4 diffuse = max(dot(-litVec, NormalW), 0.0f) * mtl.diffuse * pLight.diffuse;

		float3 posToEye = normalize(EyeW - PosW);

		float4 specular = pow(max(dot(reflect(litVec, NormalW), posToEye), 0.0f), mtl.specular.a) * mtl.specular * pLight.specular;

		float len = length(litVec);

		finalColor = (ambient + diffuse + specular) / (len * len); 
}

// end of struct definition

cbuffer cbPerWorld {
	DirectionalLight dLight;
	PointLight pLight;
	float4 eyePos;
};

cbuffer cbPerObject {
	float4x4 gWorldViewProj;
	float4x4 gWorld; // assume it is orthgonal
	Material gMaterial;
};

struct VertexIn {
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
};

struct VertexOut {
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

struct VertexOut2 {
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vertex) {
	VertexOut vout;

	vout.PosH = mul(float4(vertex.PosL, 1.0f), gWorldViewProj);

	float4 posW = mul(float4(vertex.PosL, 1.0f), gWorld);
		float3 NormalW = mul(vertex.NormalL, (float3x3)gWorld); // assume gWorld is right

		float4 dL, pL;
	CalculateColorCausedByDirectionalLight(gMaterial, (float3)posW, (float3)eyePos, NormalW, dLight, dL);
	CalculateColorCausedByPointLight(gMaterial, (float3)posW, (float3)eyePos, NormalW, pLight, pL);
	vout.Color = dL + pL;
	vout.Color.a = gMaterial.diffuse.a;

	return vout;
}

float4 PS(VertexOut vout) : SV_Target{
	return vout.Color;
}

VertexOut2 VS2(VertexIn vertex) {
	VertexOut2 vout;
	vout.PosH = mul(float4(vertex.PosL, 1.0f), gWorldViewProj);
	vout.NormalW = mul(vertex.NormalL, (float3x3)gWorld); //assume gWorld is orthgonal
	vout.PosW = (float3)mul(float4(vertex.PosL, 1.0f), gWorld);

	return vout;
}

float4 PS2(VertexOut2 vout) : SV_Target{
	float3 nw = normalize(vout.NormalW);

	float4 dL, pL; 
	CalculateColorCausedByDirectionalLight(gMaterial, vout.PosW, (float3)eyePos, nw, dLight, dL);
	CalculateColorCausedByPointLight(gMaterial, vout.PosW, (float3)eyePos, nw, pLight, pL);

	float4 result = dL + pL;
	result.a = gMaterial.diffuse.a;
	return result;
}

technique11 ColorTech {
	pass P0 {
		SetVertexShader(CompileShader(vs_5_0, VS2()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS2()));
	}
};