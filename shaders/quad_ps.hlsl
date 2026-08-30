struct PS_INPUT {
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};
sampler TexSampler : register(s0);
float4 main(PS_INPUT input) : COLOR0 {
    float4 texColor = tex2D(TexSampler, input.TexCoord);
    return texColor * input.Color;
}
