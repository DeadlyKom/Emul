#define ATTRIBUTE_GRID          1 << 0
#define GRID					1 << 1
#define PIXEL_GRID              1 << 2
#define FORCE_NEAREST_SAMPLING  1 << 31

cbuffer pixelBuffer : register(b0)
{
    float4   GridColor;
    float2   GridWidth;
    int      Flags;
    float    TimeCounter;
    float3   BackgroundColor;
    int      Dummy_0;
    float2   TextureSize;
    float2   GridSize;
    float2   GridOffset;

    float Dummy[46];
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

sampler Sampler0;
Texture2D Texture0;

float4 main(PS_INPUT Input) : SV_TARGET
{
    float2 UV;

    float2 Texel = (Input.uv) * TextureSize;
    if (Flags & FORCE_NEAREST_SAMPLING)
	    UV = (floor(Texel) + float2(0.5, 0.5)) / TextureSize;
    else
	    UV = Input.uv;

    float2 TexelEdge = step(Texel - floor(Texel), GridWidth);
    float IsGrid = max(TexelEdge.x, TexelEdge.y);
    float4 C = Texture0.Sample(Sampler0, UV);
    C.rgb += BackgroundColor * (1.0 - C.a);

    float2 uv_g = UV;
    uv_g.y *= TextureSize.y / TextureSize.x;
    float Repeats = floor(TextureSize.x / 8);
    float cx = floor(Repeats * uv_g.x);
    float cy = floor(Repeats * uv_g.y);
    float result = fmod(cx + cy, 2.0);
    float gray = Flags & ATTRIBUTE_GRID ? lerp(0.8, 1.0f, sign(result)) : 1.0f;

    float2 TexelA = (Input.uv - GridOffset / TextureSize) * TextureSize / GridSize;
    float2 GridSizeWidth = GridWidth / GridSize;
    float2 TexelEdgeA = step(TexelA - floor(TexelA), GridSizeWidth * 1.8f);
    float IsGridA = max(TexelEdgeA.x, TexelEdgeA.y);
    float4 GridColorA = float4(0,0,1,0.45);

    C = lerp(C * gray, float4(GridColor.rgb, 1), GridColor.a * (IsGrid * !!(Flags & PIXEL_GRID)));
    C = lerp(C, float4(GridColorA.rgb, 1), GridColorA.a * (IsGridA * !!(Flags & GRID)));
    return C;
}
