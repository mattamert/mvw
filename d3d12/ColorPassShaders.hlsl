cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
  float4x4 shadowMapProjectionViewTransform;
}

cbuffer PerObjectData : register(b1) {
  float4x4 worldTransform;
}

Texture2D shadowMap : register(t0);
Texture2D objectTexture : register(t1);
SamplerState aniSampler : register(s0);
SamplerState pointClamp : register(s1);

struct PSInput {
  float4 position : SV_POSITION;
  float2 tex : TEXCOORD;
  float3 normal : NORMAL;
  float4 shadowMapTexCoord : TEXCOORD1;
};

PSInput VSMain(float3 pos : POSITION, float2 tex : TEXCOORD, float3 normal : NORMAL) {
  float4x4 modelViewProjection = mul(projectionViewTransform, worldTransform);
  float4x4 shadowCameraModelViewProjection = mul(shadowMapProjectionViewTransform, worldTransform);

  PSInput result;
  result.position = mul(modelViewProjection, float4(pos, 1.f));
  result.tex = tex;
  result.normal = normal;
  result.shadowMapTexCoord = mul(shadowCameraModelViewProjection, float4(pos, 1.f));
  return result;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float4 texValue = objectTexture.Sample(aniSampler, input.tex);
  if (texValue.w == 0.f) {
    discard;
  }

  input.shadowMapTexCoord.xyz /= input.shadowMapTexCoord.w;

  float currentDepth = input.shadowMapTexCoord.z;
  float visibleDepth = shadowMap.Sample(pointClamp, float2((input.shadowMapTexCoord.x + 1) / 2, 1 - (input.shadowMapTexCoord.y + 1) / 2));
  if (currentDepth > visibleDepth)
    texValue.xyz *= 0.5;

  return texValue;
}
