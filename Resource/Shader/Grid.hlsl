#define ATTRIBUTE_GRID          1 << 0
#define GRID					1 << 1
#define PIXEL_GRID              1 << 2
#define BEAM_ENABLE             1 << 3
#define ALPHA_TRANSPARENT       1 << 4
#define ALPHA_CHECKERBOARD_GRID 1 << 5
#define PIXEL_CURSOR            1 << 6
#define ONLY_NEAREST_SAMPLING   1 << 30
#define FORCE_NEAREST_SAMPLING  1 << 31

cbuffer pixelBuffer : register(b0)
{
    float4 GridColor;
    float4 CursorColor;
    float4 TransparentColor;
    float2 GridWidth;
    int    Flags;
    float  TimeCounter;
    float3 BackgroundColor;
    int    Dummy_0;
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

Texture2D Texture0 : register(t0);
SamplerState Sampler0 : register(s0);

float4 main(PS_INPUT Input) : SV_TARGET
{
    float2 UV;

    float2 Texel = (Input.uv) * TextureSize;
    if (Flags & FORCE_NEAREST_SAMPLING)
    {
        UV = (floor(Texel) + float2(0.5, 0.5)) / TextureSize;
    }
    else
    {
        UV = Input.uv;
    }
    
    float2 TexelEdge = step(Texel - floor(Texel), GridWidth);
    float IsGrid = max(TexelEdge.x, TexelEdge.y);
    float4 SimpleColor = Texture0.Sample(Sampler0, UV);
    
    if (Flags & ONLY_NEAREST_SAMPLING)
    {
        return SimpleColor;
    }
    
    float4 ResultColor = SimpleColor;
    ResultColor.rgb += BackgroundColor * (1.0 - ResultColor.a);

    float2 UV_g = UV;
    UV_g.y *= TextureSize.y / TextureSize.x;
    const float Repeats = floor(TextureSize.x / 8);
    const float cx = floor(Repeats * UV_g.x);
    const float cy = floor(Repeats * UV_g.y);
    const float Grid_Attribute = fmod(cx + cy, 2.0);
    const float Gray = Flags & ATTRIBUTE_GRID ? lerp(1.0, 0.8f, sign(Grid_Attribute)) : 1.0f;

    // Grid thickness in raster pixels.
    const float GridThickness = 2.0f;

    // Calculate cell-grid edges using the specified thickness.
    const float2 TexelA = (Input.uv - GridOffset / TextureSize) * TextureSize / GridSize;
    const float2 GridSizeWidth = fwidth(TexelA);
    const float2 TexelEdgeA = 1.0f - step(GridSizeWidth * GridThickness, TexelA - floor(TexelA));
    const float IsGridA = max(TexelEdgeA.x, TexelEdgeA.y);

    // Calculate grid opacity from zoom, keeping the minimum observed at 50 percent and below.
    const float GridOpacity = clamp(40.0f / (255.0f * GridWidth.y), 20.0f / 255.0f, 1.0f);
    const float4 GridColorA = float4(0.0f, 0.0f, 1.0f, GridOpacity);

    // Check whether the pixel must be blended with the transparency background.
    if ((Flags & ALPHA_TRANSPARENT) && ResultColor.a < 1)
    {
        // Check whether the transparency background uses a checkerboard.
        if (Flags & ALPHA_CHECKERBOARD_GRID)
        {
            const float3 DarkColor = TransparentColor * 0.8f;
            const float3 LightColor = TransparentColor * 1.0f;
        
            // Calculate the checkerboard cell from 16 by 16 image pixels.
            const float cx = floor(UV.x * TextureSize.x / 16.0f);
            const float cy = floor(UV.y * TextureSize.y / 16.0f);
            const float Grid_Alpha = fmod(cx + cy, 2.0);
        
            ResultColor.rgb = lerp(LightColor, DarkColor, sign(Grid_Alpha));
        }
        else
        {
            ResultColor.rgb = TransparentColor;
        }

        // Blend the original pixel with the selected background using its alpha.
        ResultColor.rgb = lerp(ResultColor.rgb, SimpleColor.rgb, SimpleColor.a);
        ResultColor.a = 1.0;
    }
    else
    {
        ResultColor = lerp(ResultColor * Gray, float4(GridColor.rgb, 1), GridColor.a * (IsGrid * !!(Flags & PIXEL_GRID)));
    }
    
    if ((Flags & BEAM_ENABLE) && (CRT_BeamPosition.y > 0 || CRT_BeamPosition.x > 0))
    {
        if (UV.y > CRT_BeamPosition.y)
        {
            const float HeightBeam = 1.0f / TextureSize.y;
            if (UV.y > CRT_BeamPosition.y + HeightBeam)
            {
                ResultColor.a *= 0.5f;
            }
            else if (UV.x > CRT_BeamPosition.x)
            {
                ResultColor.a *= 0.5f;
            }
        }
    }
    ResultColor = lerp(ResultColor, float4(GridColorA.rgb, 1), GridColorA.a * (IsGridA * !!(Flags & GRID)));
    
    if (Flags & PIXEL_CURSOR)
    {
        const float2 PixelSize = 1.0f / TextureSize;
        float2 _CursorPosition = CursorPosition - PixelSize * 0.5f;
        if (UV.x >= _CursorPosition.x &&
            UV.y >= _CursorPosition.y &&
            (UV.x - PixelSize.x * 1) < _CursorPosition.x &&
            (UV.y - PixelSize.y * 1) < _CursorPosition.y)
            {
            ResultColor = CursorColor;
        }
    }
    
    if (UV.x < 0.0f ||
        UV.x > 1.0f ||
        UV.y < 0.0f ||
        UV.y > 1.0f)
    {
        ResultColor = float4(0.25f, 0.25f, 0.25f, 1.0f);
    }
    
    return ResultColor;
}