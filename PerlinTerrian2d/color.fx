
// simply transform the position to proj space and copy color//

cbuffer cbPerObject {
	float4x4 gWorldViewProj;
};

struct VertexIn {
	float3 PosL : POSITION;
	float4 Color : COLOR;
};

struct VertexOut {
	float4 PosH : SV_POSITION;
	float4 Color : COLOR;
};

VertexOut VS(VertexIn vertex) {
	VertexOut vout;

	vout.PosH = mul(float4(vertex.PosL, 1.0f), gWorldViewProj);
	vout.Color = vertex.Color;

	return vout;
}

float4 PS(VertexOut vout) : SV_Target{
	return vout.Color;
}

technique11 ColorTech {
	pass P0 {
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
};