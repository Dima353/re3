const char *contrast_f_cg_src =
"#define u_fogStart (u_fogData.x)\n"
"#define u_fogEnd (u_fogData.y)\n"
"#define u_fogRange (u_fogData.z)\n"
"#define u_fogDisable (u_fogData.w)\n"

"void DoAlphaTest(float a, half2 u_alphaRef)\n"
"{\n"
"	if(a < u_alphaRef.x || a >= u_alphaRef.y)\n"
"		discard;\n"
"}\n"

"float4 main(\n"
"	half4 v_color : COLOR0,\n"
"	half2 v_tex0 : TEXCOORD0,\n"
"	fixed v_fog : FOG,\n"
"	uniform half4 u_fogColor,\n"
"	uniform half2 u_alphaRef,\n"
"	uniform float3 u_contrastAdd,\n"
"	uniform float3 u_contrastMult,\n"
"	uniform sampler2D tex0\n"
") {\n"
"	half4 dst = v_color*tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));\n"
"	dst.rgb = lerp(u_fogColor.rgb, dst.rgb, v_fog);\n"
"	DoAlphaTest(dst.a, u_alphaRef);\n"

"	half4 color;\n"
"	color.rgb = dst.rgb*u_contrastMult + u_contrastAdd;\n"
"	color.a = 1.0f;\n"

"	return color;\n"
"}\n"


;
