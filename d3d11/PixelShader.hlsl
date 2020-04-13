float4 PSMain(
    float4 posH : SV_POSITION,
    float3 color : COLOR) : SV_TARGET {
  return float4(color, 1.0f);
}
