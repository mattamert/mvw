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
SamplerState texSampler : register(s0);
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
  float2 imageSize = float2(512.f, 512.f);
  float2 textureXY = input.tex * imageSize;

  int2 textureXYWholePart = trunc(textureXY);
  float2 textureXYFracPart = frac(textureXY);

  int2 adjustedTexXY = textureXYWholePart;
  bool isXOdd = (textureXYWholePart.x % 2 == 1);
  bool isYEven = (textureXYWholePart.y % 2 == 0);
  if (isXOdd) {
    if (textureXYFracPart.x < 0.4) {
      adjustedTexXY.x--;
    } else if (textureXYFracPart.x > 0.6) {
      adjustedTexXY.x++;
    }
  }

  if (isYEven) {
    if (textureXYFracPart.y < 0.4) {
      adjustedTexXY.y--;
    } else if (textureXYFracPart.y > 0.6) {
      adjustedTexXY.y++;
    }
  }

  float2 adjustedUV = (float2(adjustedTexXY.xy) + float2(0.5f, 0.5f)) / 512.f;
  float4 texValue = objectTexture.Sample(texSampler, adjustedUV);

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
