cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
  float4x4 shadowMapProjectionViewTransform;
}

cbuffer PerObjectData : register(b1) {
  float4x4 worldTransform;
  float4x4 worldTransformInverseTranspose;
}

Texture2D shadowMap : register(t0);
Texture2D objectTexture : register(t1);
SamplerState aniSampler : register(s0);
SamplerComparisonState pointClampComp : register(s1);

struct PSInput {
  float4 position : SV_POSITION;
  float2 tex : TEXCOORD;
  float4 normal : NORMAL;
  float4 shadowMapPos : TEXCOORD1;
};

PSInput VSMain(float3 pos : POSITION, float2 tex : TEXCOORD, float3 normal : NORMAL) {
  float4x4 modelViewProjection = mul(projectionViewTransform, worldTransform);
  float4x4 shadowCameraModelViewProjection = mul(shadowMapProjectionViewTransform, worldTransform);

  PSInput result;
  result.position = mul(modelViewProjection, float4(pos, 1.f));
  result.tex = tex;
  result.shadowMapPos = mul(shadowCameraModelViewProjection, float4(pos, 1.f));

  // TODO: This isn't robust, it only works when the transformation matrix is orthogonal.
  //       We would ideally want to multiply it by the transpose of the inverse.
  //       For now, we can guarantee it's orthogonal, but that may change in the future.
  //result.normal.xyz = normalize(mul(worldTransform, float4(normal, 1.f)).xyz);
  result.normal = float4(normal, 1);
  //result.normal = normal;

  return result;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float4 texValue = objectTexture.Sample(aniSampler, input.tex);
  //if (texValue.w == 0.f) {
  //  discard;
  //}

  float4 norm = mul(worldTransformInverseTranspose, input.normal);
  norm = float4(normalize(norm.xyz), 1.f);

  float lambertFactor = dot(norm.xyz, normalize(float3(-1, 1, 1)));

  //if (dot(norm.xyz, normalize(float3(-1, 1, 1))) < 0) {
  //  texValue.xyz *= 0.5;
  //} else {
    float2 shadowMapTexCoord = float2((input.shadowMapPos.x + 1) / 2, 1 - (input.shadowMapPos.y + 1) / 2);
    float depthInShadowMap = input.shadowMapPos.z;

    float visibility = shadowMap.SampleCmpLevelZero(pointClampComp, shadowMapTexCoord, depthInShadowMap);
    visibility = (visibility + 1) / 2;

  //}

  float shadowAmount = clamp(lambertFactor, 0.5, visibility);
  texValue *= shadowAmount;

  return texValue;

  //return norm;
}
