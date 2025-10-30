cbuffer pixelBuffer : register(b0)
{
    float4 GridColor;
    float4 CursorColor;
    float4 TransparentColor;
    float2 GridWidth;
    int Flags;
    float TimeCounter;
    float3 BackgroundColor;
    int Dummy_0;
    float2 TextureSize;
    float2 GridSize;
    float2 GridOffset;
    float2 CRT_BeamPosition;
    float2 CursorPosition;

    float Dummy[34];
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

Texture2D Texture0;

float4 main(PS_INPUT Input) : SV_TARGET
{
    float C = ((int)(Input.pos.x + Input.pos.y + TimeCounter * 16) % 8);
    return (C < 4 ? float4(0, 0, 0, 1) : float4(1, 1, 1, 1));
}
