#ifndef __SSRCOMMON_HLSLI__
#define __SSRCOMMON_HLSLI__

cbuffer cbSsr : register(b0) {
	float4x4	gView;
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float3		gEyePosW;
	float		gMaxDistance;
	float		gRayLength;
	float		gNoiseIntensity;
	int			gNumSteps;
	int			gNumBackSteps;
	float		gDepthThreshold;
	float		gConstantPad0;
	float		gConstantPad1;
	float		gConstantPad2;
};

#endif // __SSRCOMMON_HLSLI__