#define ATTRIBUTE_GRID          1 << 0
#define GRID					1 << 1
#define PIXEL_GRID              1 << 2
#define BEAM_ENABLE             1 << 3
#define ALPHA_TRANSPARENT       1 << 4
#define ALPHA_CHECKERBOARD_GRID 1 << 5
#define PIXEL_CURSOR            1 << 6
#define PIXEL_COVERAGE_SAMPLING 1 << 7
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

float4 SamplePixelCoverage(float2 Texel)
{
    // Calculate the source-image area covered by one screen pixel.
    const float2 PixelWidth = max(fwidth(Texel), float2(0.000001f, 0.000001f));
    float4 Result = 0.0f;
    float TotalWeight = 0.0f;

    // Accumulate source-pixel coverage.
    {
        const float2 PixelMin = Texel - PixelWidth * 0.5f;
        const float2 PixelMax = Texel + PixelWidth * 0.5f;

        // Calculate the source-pixel range intersecting this area.
        const int2 FirstTexel = int2(floor(PixelMin));
        const int2 LastTexel = int2(ceil(PixelMax));

        // Accumulate each source pixel in proportion to its covered area.
        [loop]
        for (int Y = FirstTexel.y; Y < LastTexel.y; ++Y)
        {
            [loop]
            for (int X = FirstTexel.x; X < LastTexel.x; ++X)
            {
                // Calculate the overlap between this source pixel and the screen pixel.
                const float2 CellMin = float2(X, Y);
                const float2 Coverage = max(min(PixelMax, CellMin + 1.0f) - max(PixelMin, CellMin), 0.0f);
                const float Weight = Coverage.x * Coverage.y;

                // Read the source color with the texture's existing clamp-edge behavior.
                const int2 TexelIndex = clamp(int2(X, Y), int2(0, 0), int2(TextureSize) - 1);
                const float4 Color = Texture0.Load(int3(TexelIndex, 0));

                // Weight RGB by alpha so transparent pixels cannot create colored fringes.
                Result += float4(Color.rgb * Color.a, Color.a) * Weight;
                // Accumulate the same coverage weights used for the color and alpha.
                TotalWeight += Weight;
            }
        }
    }

    // Normalize the accumulated color and alpha.
    {
        // Check whether visible color was accumulated before restoring straight RGB.
        if (Result.a > 0.0f)
        {
            // Calculate the visible color independently of transparent coverage.
            Result.rgb /= Result.a;
        }

        // Normalize alpha by actual coverage, or use zero when no area was covered.
        Result.a = TotalWeight > 0.0f ? saturate(Result.a / TotalWeight) : 0.0f;
    }

    return Result;
}

float4 main(PS_INPUT Input) : SV_TARGET
{
    float2 UV;
    float IsGrid;
    float4 SimpleColor;

    // Sample the source image.
    {
        float2 Texel = (Input.uv) * TextureSize;
        if (Flags & FORCE_NEAREST_SAMPLING)
        {
            UV = (floor(Texel) + float2(0.5, 0.5)) / TextureSize;
        }
        else
        {
            UV = Input.uv;
        }

        // Draw one-screen-pixel grid lines without smoothing their edges.
        {
            // Calculate the nearest source-cell boundary, including one beyond this pixel center.
            const float2 PixelWidth = max(fwidth(Texel), float2(0.000001f, 0.000001f));
            const float2 GridPosition = Input.pos.xy + (round(Texel) - Texel) / PixelWidth;

            // Stabilize the boundary, then select the screen pixel whose area it crosses.
            const float2 GridPixel = floor(round(GridPosition * 256.0f) / 256.0f);

            // Check whether this screen pixel belongs to either grid direction.
            const float2 GridDistance = abs(floor(Input.pos.xy) - GridPixel);
            IsGrid = 1.0f - step(0.5f, min(GridDistance.x, GridDistance.y));
        }
        SimpleColor = Texture0.Sample(Sampler0, UV);

        if (Flags & ONLY_NEAREST_SAMPLING)
        {
            return SimpleColor;
        }

        // Check whether this canvas uses area coverage for source-image pixels.
        if (Flags & PIXEL_COVERAGE_SAMPLING)
        {
            // Average the original texel footprint before applying display overlays.
            SimpleColor = SamplePixelCoverage(Texel);
        }
    }

    float4 ResultColor = SimpleColor;
    // Apply the background color.
    {
        ResultColor.rgb += BackgroundColor * (1.0 - ResultColor.a);
    }

    float Gray;

    // Calculate attribute-cell shading.
    {
        float2 UV_g = UV;
        UV_g.y *= TextureSize.y / TextureSize.x;
        const float Repeats = floor(TextureSize.x / 8);
        const float cx = floor(Repeats * UV_g.x);
        const float cy = floor(Repeats * UV_g.y);
        const float Grid_Attribute = fmod(cx + cy, 2.0);
        Gray = Flags & ATTRIBUTE_GRID ? lerp(1.0, 0.8f, sign(Grid_Attribute)) : 1.0f;
    }

    float IsGridA;
    float4 GridColorA;

    // Calculate cell-grid coverage and opacity.
    {
        // Grid thickness in raster pixels.
        const float GridThickness = 1.5f;

        // Calculate grid coordinates and the interval covered by one screen pixel.
        const float2 TexelA = (Input.uv - GridOffset / TextureSize) * TextureSize / GridSize;
        const float2 GridSizeWidth = max(fwidth(TexelA), float2(0.000001f, 0.000001f));
        const float2 GridPixelMin = frac(TexelA) - GridSizeWidth * 0.5f;
        const float2 GridPixelMax = GridPixelMin + GridSizeWidth;

        // Calculate line width in cell coordinates, capped at one complete cell.
        const float2 GridLineWidth = saturate(GridSizeWidth * GridThickness);

        // Integrate the repeating lines at both pixel edges, including cell-boundary crossings.
        const float2 GridCoverageMin = floor(GridPixelMin) * GridLineWidth + min(frac(GridPixelMin), GridLineWidth);
        const float2 GridCoverageMax = floor(GridPixelMax) * GridLineWidth + min(frac(GridPixelMax), GridLineWidth);

        // Calculate the fraction of the pixel covered by each line direction.
        const float2 TexelEdgeA = saturate((GridCoverageMax - GridCoverageMin) / GridSizeWidth);

        // Combine horizontal and vertical coverage without counting their intersection twice.
        IsGridA = 1.0f - (1.0f - TexelEdgeA.x) * (1.0f - TexelEdgeA.y);

        // Calculate grid opacity from zoom, keeping the minimum observed at 50 percent and below.
        const float GridOpacity = clamp(40.0f / (255.0f * GridWidth.y), 20.0f / 255.0f, 1.0f);
        GridColorA = float4(0.0f, 0.0f, 1.0f, GridOpacity);
    }

    // Composite transparency.
    {
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
    }

    // Overlay attribute cells on both the image and the transparency checkerboard.
    {
        // Blend RGB toward neutral gray so black cells remain distinguishable without changing alpha.
        ResultColor.rgb = lerp(ResultColor.rgb, float3(0.5f, 0.5f, 0.5f), 1.0f - Gray);
    }

    // Apply display overlays and canvas bounds.
    {
        // Draw the beam overlay.
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
        // Draw the cell grid.
        ResultColor = lerp(ResultColor, float4(GridColorA.rgb, 1), GridColorA.a * (IsGridA * !!(Flags & GRID)));

        // Overlay the pixel grid after the image and the custom grid have been composited.
        {
            // Draw over the completed image using only the grid's own opacity.
            ResultColor = lerp(ResultColor, float4(GridColor.rgb, 1), GridColor.a * (IsGrid * !!(Flags & PIXEL_GRID)));
        }

        // Draw the pixel cursor.
        if (Flags & PIXEL_CURSOR)
        {
            const float2 PixelSize = 1.0f / TextureSize;
            float2 _CursorPosition = CursorPosition - PixelSize * 0.5f;
            if (UV.x >= _CursorPosition.x && UV.y >= _CursorPosition.y && (UV.x - PixelSize.x * 1) < _CursorPosition.x && (UV.y - PixelSize.y * 1) < _CursorPosition.y)
            {
                ResultColor = CursorColor;
            }
        }

        // Replace pixels outside the canvas with the surrounding background.
        if (UV.x < 0.0f || UV.x > 1.0f || UV.y < 0.0f || UV.y > 1.0f)
        {
            ResultColor = float4(0.25f, 0.25f, 0.25f, 1.0f);
        }
    }

    return ResultColor;
}