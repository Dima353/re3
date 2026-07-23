const char *colourfilterVC_f_cg_src =
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
"	uniform half4 u_blurcolor,\n"
"	uniform sampler2D tex0\n"
") {\n"
"	half a = u_blurcolor.a;\n"
"	half4 doublec = clamp(u_blurcolor*2.0, 0.0, 1.0);\n"
"	half4 dst = v_color*tex2D(tex0, half2(v_tex0.x, 1.0-v_tex0.y));\n"
"	dst.rgb = lerp(u_fogColor.rgb, dst.rgb, v_fog);\n"
"	DoAlphaTest(dst.a, u_alphaRef);\n"

"	half4 prev = dst;\n"
"	for (int i = 0; i < 5; i++){\n"
"		half4 tmp = dst*(1.0-a) + prev*doublec*a;\n"
"		tmp += prev*u_blurcolor;\n"
"		tmp += prev*u_blurcolor;\n"
"		prev = clamp(tmp, 0.0, 1.0);\n"
"	}\n"
"	half4 color;\n"
"	color.rgb = prev.rgb;\n"
"	color.a = 1.0f;\n"

"	return color;\n"
"}\n"


;
