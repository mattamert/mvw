//cbuffer per_object_animation : register(b0) {
//  float additional_red;
//};

//cbuffer per_object_matrices : register(b1) {
cbuffer per_frame_data : register(b0) {
  row_major float4x4 view_perspective_transform;
};

cbuffer per_object_data : register(b1) {
  row_major float4x4 world_transform;
}

void VSMain(
    float3 iPos : POSITION,
    float3 iColor : COLOR,
    out float4 oPos : SV_POSITION,
    out float3 oColor : COLOR) {
  //oPos = mul(float4(iPos, 1.0f), mul(world_transform, view_perspective_transform));
  row_major float4x4 mvp_matrix = mul(world_transform, view_perspective_transform);
  oPos = mul(float4(iPos, 1.0f), mvp_matrix);
  //oPos = mul(world_view_perspective_transform, float4(iPos, 1.f));

  //iColor *= additional_red;
  oColor = (iColor + 1) / 2;
}
