#ifndef __BRDF_DECLARATIONS_HLSLI__
#define __BRDF_DECLARATIONS_HLSLI__

// Trowbridge-Reitz GGX 
float DistributionGGX(float3 N, float3 H, float roughness);
float DistributionGGX_Modified(float3 N, float3 H, float roughness, float d, float radius);

// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float roughness);
float GeometryShlickGGX_IBL(float NdotV, float roughness);

float GeometrySmith(float3 N, float3 V, float3 L, float roughness);
float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness);

// the Fresnel Schlick approximation
float3 FresnelSchlick(float cos, float3 F0);
float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness);

#endif // __BRDF_DECLARATIONS_HLSLI__