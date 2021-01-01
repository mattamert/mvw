cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
}

cbuffer PerObjectData : register(b1) {
  float4x4 worldTransform;
}

Texture2D shadowMap     : register(t0);
Texture2D objectTexture : register(t1);
SamplerState pointClamp : register(s0);

struct PSInput {
  float4 position : SV_POSITION;
  float2 tex : TEXCOORD;
  float3 normal : NORMAL;
};

PSInput VSMain(float3 pos : POSITION, float2 tex : TEXCOORD, float3 normal : NORMAL) {
  float4x4 modelViewProjection = mul(projectionViewTransform, worldTransform);

  PSInput result;
  result.position = mul(modelViewProjection, float4(pos, 1.f));
  result.tex = tex;
  result.normal = normal;
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float4 texValue = objectTexture.Sample(pointClamp, input.tex, 1.0f);
  if (texValue.w == 0.f) {
    discard;
  }

  return texValue;
}
