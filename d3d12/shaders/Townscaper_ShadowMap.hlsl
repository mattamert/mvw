cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
}

cbuffer PerObjectData : register(b1) {
  float4x4 worldTransform;
}

Texture2D objectTexture : register(t0);
SamplerState texSampler : register(s0);

struct PSInput {
  float4 position : SV_POSITION;
  float2 tex : TEXCOORD;
};

PSInput VSMain(float3 pos : POSITION, float2 tex : TEXCOORD) {
  float4x4 modelViewProjection = mul(projectionViewTransform, worldTransform);

  PSInput result;
  result.position = mul(modelViewProjection, float4(pos, 1.f));
  result.tex = tex;
  return result;
}

void PSMain(PSInput input) {
  // We only need the depth, so we don't need to do anything else.
  float4 texValue = objectTexture.Sample(texSampler, input.tex);
  if (texValue.w == 0.f) {
    discard;
  }
}
