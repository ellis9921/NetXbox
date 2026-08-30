struct VS_INPUT {
    float4 Position : POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};
struct VS_OUTPUT {
    float4 Position : POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = input.Position;
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    return output;
}
