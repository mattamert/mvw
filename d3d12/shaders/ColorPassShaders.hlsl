cbuffer PerFrameData : register(b0) {
  float4x4 projectionViewTransform;
  float4x4 shadowMapProjectionViewTransform;
  float4 lightDirection;
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

  // Normals shouldn't be used as homogeneous since they don't have a position. We therefore use 0 as the w component.
  // We can simplify this further by instead just using a 3x3 matrix.
  result.normal = normalize(mul(worldTransformInverseTranspose, float4(normal, 0)));

  return result;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float4 texValue = objectTexture.Sample(aniSampler, input.tex);
  if (texValue.w == 0.f) {
    discard;
  }

  // TODO: Don't hard-code light direction. Make it a parameter in the PerFrameData buffer.
  float lambertFactor = dot(normalize(input.normal.xyz), normalize(lightDirection.xyz));

  float2 shadowMapTexCoord =
      float2((input.shadowMapPos.x + 1) / 2, 1 - ((input.shadowMapPos.y + 1) / 2));
  float depthInShadowMap = input.shadowMapPos.z;
  float visibility =
      shadowMap.SampleCmpLevelZero(pointClampComp, shadowMapTexCoord, depthInShadowMap);

  visibility = (visibility + 1) / 2;

  float shadowAmount = clamp(lambertFactor, 0.5, visibility);
  texValue *= shadowAmount;

  return texValue;
}
