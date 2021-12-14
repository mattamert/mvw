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

// TODO: We really should check if we're in a grout, and if so, how far we are away from the nearest edge.
float2 GetXYDistanceToGrout(float2 textureXY) {
  int2 textureXYWholePart = trunc(textureXY);
  float2 textureXYFracPart = frac(textureXY);
  
  bool isXOdd = (textureXYWholePart.x % 2 == 1);
  bool isYEven = (textureXYWholePart.y % 2 == 0);

  float2 distance;
  if (isXOdd) {
    float distanceFromCenterX = abs(textureXYFracPart.x - 0.5);
    distance.x = clamp(distanceFromCenterX - 0.1, 0, 1);
  } else {
    float distanceFromNearestEdgeX = 0.5 - abs(textureXYFracPart.x - 0.5);
    distance.x = distanceFromNearestEdgeX + 0.4;
  }

  if (isYEven) {
    float distanceFromCenterY = abs(textureXYFracPart.y - 0.5);
    distance.y = clamp(distanceFromCenterY - 0.1, 0, 1);
  } else {
    float distanceFromNearestEdgeY = 0.5 - abs(textureXYFracPart.y - 0.5);
    distance.y = distanceFromNearestEdgeY + 0.4;
  }

  return distance;
}

//float4 PSMain_TEST(PSInput input) : SV_TARGET{
float4 PSMain(PSInput input) : SV_TARGET{
  float2 dxUV = ddx(input.tex);
  //float2 dyUV = ddy(input.tex);

  float2 imageSize = float2(512.f, 512.f);
  float2 textureXY = input.tex * imageSize;
  float2 pixelSizeXY = dxUV * imageSize;

  int2 textureXYWholePart = trunc(textureXY);
  float2 textureXYFracPart = frac(textureXY);

  bool isXOdd = (textureXYWholePart.x % 2 == 1);
  bool isYEven = (textureXYWholePart.y % 2 == 0);

  int2 tileColorXY = textureXYWholePart;
  int2 groutColorXY = textureXYWholePart;
  if (isXOdd) {
    tileColorXY.x += (textureXYFracPart.x < 0.5) ? -1 : 1;
  }

  if (isYEven) {
    tileColorXY.y += (textureXYFracPart.y < 0.5) ? -1 : 1;
  }

  if (isXOdd && isYEven) {
    //bool isGroutX = (textureXYFracPart.x >= 0.4 && textureXYFracPart.x <= 0.6);
    //bool isGroutY = (textureXYFracPart.y >= 0.4 && textureXYFracPart.y <= 0.6);
    //if (isGroutX && !isGroutY) {
    //  if (textureXYFracPart.y < 0.5) {
    //    groutColorXY.y--;
    //  } else {
    //    groutColorXY.y++;
    //  }
    //}
    //else if (!isGroutX && isGroutY) {
    //  if (textureXYFracPart.x < 0.5) {
    //    groutColorXY.x--;
    //  } else {
    //    groutColorXY.x++;
    //  }
    //}

    //groutColorXY.x += (textureXYFracPart.x < 0.5) ? -1 : 1;
    //groutColorXY.y += (textureXYFracPart.y < 0.5) ? -1 : 1;
    if (textureXYFracPart.x > textureXYFracPart.y) {
      if (1.f - textureXYFracPart.x > textureXYFracPart.y) {
        groutColorXY.y--;
      } else {
        groutColorXY.x++;
      }
    } else {
      if (1.f - textureXYFracPart.x > textureXYFracPart.y) {
        groutColorXY.x--;
      } else {
        groutColorXY.y++;
      }
    }
  }

  float2 distanceToGroutXY = GetXYDistanceToGrout(textureXY);
  float2 lerpValues = saturate(distanceToGroutXY / abs(pixelSizeXY));
  //float2 distanceToGroutUV = distanceToGroutXY / 512.f;
  //float2 lerpValues = saturate(distanceToGroutUV / abs(pixelSizeInUVCoordinates));

  float2 tileColorUV = float2(tileColorXY.xy) / 512.f;
  float2 groutColorUV = float2(groutColorXY.xy) / 512.f;
  float4 tileColor = objectTexture.Sample(texSampler, tileColorUV);
  float4 groutColor = objectTexture.Sample(texSampler, groutColorUV);
  float4 texValue = lerp(groutColor, tileColor, min(lerpValues.x, lerpValues.y));

  float lambertFactor = dot(normalize(input.normal.xyz), normalize(lightDirection.xyz));

  float2 shadowMapTexCoord = float2((input.shadowMapPos.x + 1) / 2, 1 - ((input.shadowMapPos.y + 1) / 2));
  float depthInShadowMap = input.shadowMapPos.z;

  // Visibility is a value between 0 & 1, where 0 is fully in shadow, and 1 is fully illuminated by the light.
  float visibility = shadowMap.SampleCmpLevelZero(pointClampComp, shadowMapTexCoord, depthInShadowMap);

  // Map it to 0.75 -> 1
  visibility = (visibility + 2) / 3;

  float shadowAmount = clamp(lambertFactor, 0.66, visibility);
  texValue *= shadowAmount;

  return texValue;
}

float4 PSMain_TEST(PSInput input) : SV_TARGET{
//float4 PSMain(PSInput input) : SV_TARGET{
  float2 imageSize = float2(512.f, 512.f);
  float2 textureXY = input.tex * imageSize;

  int2 textureXYWholePart = trunc(textureXY);
  float2 textureXYFracPart = frac(textureXY);

  bool isXOdd = (textureXYWholePart.x % 2 == 1);
  bool isYEven = (textureXYWholePart.y % 2 == 0);

  float2 lerpValues = float2(1.f, 1.f);
  int2 tileColorXY = textureXYWholePart;
  int2 groutColorXY = textureXYWholePart;
  if (isXOdd) {
    //if (textureXYFracPart.x < 0.4) {
    //  groutColorXY.x--;
    //} else if (textureXYFracPart.x > 0.6) {
    //  groutColorXY.x++;
    //}

    if (textureXYFracPart.x < 0.5) {
      tileColorXY.x--;
    } else {
      tileColorXY.x++;
    }

    // Map the fractional part such that values between 0.4 & 0.6 get mapped to 0 -> 1, such that:
    //   0.4 -> 1
    //   0.5 -> 0
    //   0.6 -> 1
    //   Everything outside of this range -> 1.
    lerpValues.x = clamp(abs(textureXYFracPart.x - 0.5f) * 10, 0.f, 1.f);
  }

  if (isYEven) {
    //if (textureXYFracPart.y < 0.4) {
    //  adjustedTexXY.y--;
    //} else if (textureXYFracPart.y > 0.6) {
    //  adjustedTexXY.y++;
    //}

    if (textureXYFracPart.y < 0.5) {
      tileColorXY.y--;
    } else {
      tileColorXY.y++;
    }

    // Map the fractional part such that values between 0.4 & 0.6 get mapped to 0 -> 1, such that:
    //   0.4 -> 1
    //   0.5 -> 0
    //   0.6 -> 1
    //   Everything outside of this range -> 1.
    lerpValues.y = clamp(abs(textureXYFracPart.y - 0.5f) * 10, 0.f, 1.f);
  }

  if (isXOdd && isYEven) {
    bool isGroutX = (textureXYFracPart.x >= 0.4 && textureXYFracPart.x <= 0.6);
    bool isGroutY = (textureXYFracPart.y >= 0.4 && textureXYFracPart.y <= 0.6);
    if (isGroutX && !isGroutY) {
      if (textureXYFracPart.y < 0.5) {
        groutColorXY.y--;
      } else {
        groutColorXY.y++;
      }
    } else if (!isGroutX && isGroutY) {
      if (textureXYFracPart.x < 0.5) {
        groutColorXY.x--;
      } else {
        groutColorXY.x++;
      }
    }
  }

  float2 tileColorUV = float2(tileColorXY.xy) / 512.f;
  float4 tileColor = objectTexture.Sample(texSampler, tileColorUV);

  float2 groutColorUV = float2(groutColorXY.xy) / 512.f;
  float4 groutColor = objectTexture.Sample(texSampler, groutColorUV);

  float4 texValue = lerp(groutColor, tileColor, min(lerpValues.x, lerpValues.y));

  float lambertFactor = dot(normalize(input.normal.xyz), normalize(lightDirection.xyz));

  float2 shadowMapTexCoord =
      float2((input.shadowMapPos.x + 1) / 2, 1 - ((input.shadowMapPos.y + 1) / 2));
  float depthInShadowMap = input.shadowMapPos.z;

  // Visibility is a value between 0 & 1, where 0 is fully in shadow, and 1 is fully illuminated by the light.
  float visibility = shadowMap.SampleCmpLevelZero(pointClampComp, shadowMapTexCoord, depthInShadowMap);

  // Map it to 0.75 -> 1
  visibility = (visibility + 2) / 3;

  float shadowAmount = clamp(lambertFactor, 0.66, visibility);
  texValue *= shadowAmount;

  return texValue;
}
