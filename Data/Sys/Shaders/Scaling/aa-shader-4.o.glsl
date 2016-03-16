/*
[configuration]

[OptionRangeFloat]
GUIName = Sharpness Control
OptionName = SHARPNESS
MinValue = 1.0
MaxValue = 10.0
StepAmount = 1.0
DefaultValue = 2.0


[Pass]
Input0=ColorBuffer
Input0Mode=Clamp
Input0Filter=Linear
EntryPoint=main

[/configuration]
*/

//##############################################################//
//						                //
//       AA Shader 4.o shader - coded by guest(r)               //
//		     part of code by ShadX		        //
//##############################################################// 
// Ported by Hyllian - 2015


float3 texture2d (float2 texcoord, float4 yx) {
	const float3 dt = float3(1.0,1.0,1.0);

	float3 s00 = SampleLocation(texcoord + yx.zw).xyz; 
	float3 s20 = SampleLocation(texcoord + yx.xw).xyz; 
	float3 s22 = SampleLocation(texcoord + yx.xy).xyz; 
	float3 s02 = SampleLocation(texcoord + yx.zy).xyz; 

	float m1=dot(abs(s00-s22),dt)+0.001;
	float m2=dot(abs(s02-s20),dt)+0.001;

	return .5*(m2*(s00+s22)+m1*(s02+s20))/(m1+m2);
}


void main()
{
	const float3 dt = float3(1.0,1.0,1.0);

	// Calculating texel coordinates
	float2 size     = GetOption(SHARPNESS)*GetTargetResolution();
	float2 inv_size = 1/size;

	float4 yx = float4(inv_size, -inv_size);
	
	float2 OGL2Pos = GetCoordinates()*size;

	float2 fp = frac(OGL2Pos);
	float2 dx = float2(inv_size.x,0.0);
	float2 dy = float2(0.0, inv_size.y);
	float2 g1 = float2(inv_size.x,inv_size.y);
	float2 g2 = float2(-inv_size.x,inv_size.y);
	
	float2 pC4 = floor(OGL2Pos) * inv_size;	
	
	// Reading the texels
	float3 C0 = texture2d(pC4 - g1, yx); 
	float3 C1 = texture2d(pC4 - dy, yx);
	float3 C2 = texture2d(pC4 - g2, yx);
	float3 C3 = texture2d(pC4 - dx, yx);
	float3 C4 = texture2d(pC4     , yx);
	float3 C5 = texture2d(pC4 + dx, yx);
	float3 C6 = texture2d(pC4 + g2, yx);
	float3 C7 = texture2d(pC4 + dy, yx);
	float3 C8 = texture2d(pC4 + g1, yx);
	
	float3 ul, ur, dl, dr;
	float m1, m2;
	
	m1 = dot(abs(C0-C4),dt)+0.001;
	m2 = dot(abs(C1-C3),dt)+0.001;
	ul = (m2*(C0+C4)+m1*(C1+C3))/(m1+m2);  
	
	m1 = dot(abs(C1-C5),dt)+0.001;
	m2 = dot(abs(C2-C4),dt)+0.001;
	ur = (m2*(C1+C5)+m1*(C2+C4))/(m1+m2);
	
	m1 = dot(abs(C3-C7),dt)+0.001;
	m2 = dot(abs(C6-C4),dt)+0.001;
	dl = (m2*(C3+C7)+m1*(C6+C4))/(m1+m2);
	
	m1 = dot(abs(C4-C8),dt)+0.001;
	m2 = dot(abs(C5-C7),dt)+0.001;
	dr = (m2*(C4+C8)+m1*(C5+C7))/(m1+m2);
	
	float3 c11 = 0.5*((dr*fp.x+dl*(1-fp.x))*fp.y+(ur*fp.x+ul*(1-fp.x))*(1-fp.y) );

	SetOutput(float4(c11, 1.0));
}

