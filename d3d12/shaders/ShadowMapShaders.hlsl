cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
}

cbuffer PerObjectData : register(b1) {
  float4x4 worldTransform;
}

SamplerState pointClamp : register(s0);

struct PSInput {
  float4 position : SV_POSITION;
  float2 tex : TEXCOORD;
};

PSInput VSMain(float3 pos : POSITION) {
  float4x4 modelViewProjection = mul(projectionViewTransform, worldTransform);

  PSInput result;
  result.position = mul(modelViewProjection, float4(pos, 1.f));
  return result;
}

void PSMain(PSInput input) {
  // For now, we don't need to do anything; we only need the depth.
  // TODO: We do need to consult the texture eventually, to factor in fully transparent pixels.
}
