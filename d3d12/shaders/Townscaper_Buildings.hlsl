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

float4 SampleTownscaperBuildingTexture(float2 uv) {
  float2 imageSize = float2(512.f, 512.f);
  float2 textureXY = uv * imageSize;

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
  return objectTexture.Sample(texSampler, adjustedUV);
}

float4 CalculateLighting(float4 texColor, float visibility) {
  float4 darkColor = float4(0.49281, 0.58611, 0.68667, 1.00); // (light-blue-ish)
  float4 darkFactor = float4(1.8000, 1.8000, 1.8000, 0.0000);

  float4 lightColor = float4(0.70804, 0.61751, 0.54934, 1.00); // (Warm orange/yellow)
  float4 lightFactor = float4(1.4000, 1.4000, 1.4000, 0.0000);

  float4 darkColor_weighted = darkColor * darkFactor;
  float4 lightColor_weighted = lightColor * lightFactor;

  float4 color = (1 - visibility) * darkColor_weighted + lightColor_weighted * visibility;

  float4 minimumLightness = lightColor* visibility * float4(0.4, 0.4, 0.4, 0.f);

  // Not 100% sure what the color filter is for, or how to use it correctly here? It's a vertex attribute, and I don't
  // know what generates it.
  //float4 colorFilter = float4(0.91955, 0.77043, 0.77043, 0.84455);
  float4 colorFilter = float4(1.f, 1.f, 1.f, 0.f);
  float4 filteredColor = colorFilter * color + minimumLightness;
  filteredColor += float4(0.1, 0.1, 0.1, 0.f);

  float4 total_color = texColor * filteredColor;

  // Not really sure what this represents.
  float foo = total_color.r * 0.2 + total_color.g * 0.7 + total_color.b * 0.1;
  foo = foo * 2 - 0.6f;

  total_color += (foo * 0.1f);
  total_color = ((0.6f - total_color) * float4(-0.2f, -0.05f, 0.2f, 0.f)) + total_color;
  return total_color;
}

float4 PSMain(PSInput input) : SV_TARGET{
  float2 dx = ddx(input.tex) / 4;
  float2 dy = ddy(input.tex) / 4;

  float4 texValue = SampleTownscaperBuildingTexture(input.tex + dx + dy);
  texValue += SampleTownscaperBuildingTexture(input.tex + dx - dy);
  texValue += SampleTownscaperBuildingTexture(input.tex - dx + dy);
  texValue += SampleTownscaperBuildingTexture(input.tex - dx - dy);
  texValue /= 4;

  float2 shadowMapTexCoord = float2((input.shadowMapPos.x + 1) / 2, 1 - ((input.shadowMapPos.y + 1) / 2));
  float depthInShadowMap = input.shadowMapPos.z;

  // Visibility is a value between 0 & 1, where 0 is fully in shadow, and 1 is fully illuminated by the light.
  float visibility = shadowMap.SampleCmpLevelZero(pointClampComp, shadowMapTexCoord, depthInShadowMap);
  visibility *= 0.7;
  float4 lightedValue = CalculateLighting(texValue, visibility);
  return lightedValue;
}
