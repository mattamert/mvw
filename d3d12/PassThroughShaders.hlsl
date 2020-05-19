struct PSInput {
  float4 position : SV_POSITION;
  float3 color : COLOR;
};

PSInput VSMain(float3 pos : POSITION, float3 color : COLOR) {
  PSInput result;
  result.position = float4(pos, 1.f);
  result.color = color;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
  return float4(input.color, 1.0f);
}
