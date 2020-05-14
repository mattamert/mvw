float4 VSMain(float4 pos : POSITION) : SV_POSITION {
  return pos;
}

float4 PSMain() : SV_TARGET {
  return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
