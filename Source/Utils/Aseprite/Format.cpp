#include "Format.h"
#include "Definition.h"

// https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
namespace AsepriteFormat
{
    const uint32_t RGBA_R_SHIFT = 0;
    const uint32_t RGBA_G_SHIFT = 8;
    const uint32_t RGBA_B_SHIFT = 16;
    const uint32_t RGBA_A_SHIFT = 24;

    const uint16_t GRAYA_V_SHIFT = 0;
    const uint16_t GRAYA_A_SHIFT = 8;

    enum EPixelFormat
    {
        IMAGE_RGB,       // 32bpp see doc::rgba()
        IMAGE_GRAYSCALE, // 16bpp see doc::graya()
        IMAGE_INDEXED,   // 8bpp
        IMAGE_BITMAP_,   // 1bpp
        IMAGE_TILEMAP,   // 32bpp see doc::tile()
    };

    inline constexpr EPixelFormat PixelFormat(EColorMode CM)
    {
        return (EPixelFormat)CM;
    }

    struct FPalette
    {
        std::vector<uint32_t> RGBA;
    };

    constexpr inline int Scale6bitsTo8bits(const int32_t v)
    {
        return (v << 2) | (v >> 4);
    }

    static uint32_t RGBA(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
    {
        return ((R << RGBA_R_SHIFT) | (G << RGBA_G_SHIFT) | (B << RGBA_B_SHIFT) | (A << RGBA_A_SHIFT));
    }

    static uint16_t GrayA(uint8_t v, uint8_t a)
    {
        return ((v << GRAYA_V_SHIFT) | (a << GRAYA_A_SHIFT));
    }

    static uint8_t Read_u8(std::ifstream& File)
    {
        uint8_t b = {};
        File.read((char*)&b, 1);
        return b;
    }

    static uint16_t Read_u16(std::ifstream& File)
    {
        uint8_t b[2] = {};
        File.read((char*)b, 2);
        return b[0] | (b[1] << 8);
    }

    static uint32_t Read_u32(std::ifstream& File)
    {
        uint8_t b[4] = {};
        File.read((char*)b, 4);
        return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
    }

    static size_t ReadBytes(std::ifstream& File, uint8_t* buf, size_t n)
    {
        File.read(reinterpret_cast<char*>(buf), n);
        return File.gcount();
    }

    static void ReadPadding(std::ifstream& File, int32_t Bytes)
    {
        for (int c = 0; c < Bytes; c++)
        {
            Read_u8(File);
        }
    }

    static std::string ReadString(std::ifstream& File)
    {
        uint16_t Length = Read_u16(File);
        if (Length == EOF)
        {
            return "";
        }

        std::string String;
        String.reserve(Length + 1);

        for (uint16_t c = 0; c < Length; c++)
        {
            String.push_back(Read_u8(File));
        }
        return String;
    }

    static void ReadRawImage(std::ifstream& File, EPixelFormat PixelFormat, std::vector<uint8_t>& OutputRGBA, const FAsepriteHeader& Header, uint16_t X, uint16_t Y, uint16_t W, uint16_t H)
    {
        switch (PixelFormat)
        {
        case EPixelFormat::IMAGE_RGB:
        {
            for (uint16_t y = 0; y < H; ++y)
            {
                for (uint16_t x = 0; x < W; ++x)
                {
                    const uint8_t R = Read_u8(File);
                    const uint8_t G = Read_u8(File);
                    const uint8_t B = Read_u8(File);
                    const uint8_t A = Read_u8(File);

                    const uint32_t rgba = RGBA(R, G, B, A);

                    const size_t Index = ((Y + y) * Header.Width + X + x) * sizeof(uint32_t);
                    reinterpret_cast<uint32_t*>(&OutputRGBA[Index])[0] = rgba;
                }
            }
            break;
        }

        case EPixelFormat::IMAGE_GRAYSCALE:
        {
            for (uint16_t y = 0; y < H; ++y)
            {
                for (uint16_t x = 0; x < W; ++x)
                {
                    const uint8_t K = Read_u8(File);
                    const uint8_t A = Read_u8(File);

                    const uint16_t Grayscale = GrayA(K, A);

                    const size_t Index = ((Y + y) * Header.Width + X + x) * sizeof(uint32_t);
                    reinterpret_cast<uint32_t*>(&OutputRGBA[Index])[0] = Grayscale;
                }
            }
            break;
        }

        case EPixelFormat::IMAGE_INDEXED:
        {
            for (uint16_t y = 0; y < H; ++y)
            {
                for (uint16_t x = 0; x < W; ++x)
                {
                    const uint8_t I = Read_u8(File);
                    const size_t Index = ((Y + y) * Header.Width + X + x) * sizeof(uint32_t);
                    reinterpret_cast<uint32_t*>(&OutputRGBA[Index])[0] = I;
                }
            }
            break;
        }

        case EPixelFormat::IMAGE_TILEMAP:
        {
            // ToDo:
            break;
        }

        }
    }

    static bool ReadCompressedImage(std::ifstream& File, EPixelFormat PixelFormat, std::vector<uint8_t>& OutputRGBA, const FAsepriteHeader& Header, size_t ChunkEnd, uint16_t X, uint16_t Y, uint16_t W, uint16_t H)
    {
        z_stream zstream{};
        int32_t err = inflateInit(&zstream);
        
        std::vector<uint8_t> Compressed(4096);
        std::vector<uint8_t> Uncompressed(4096);

        switch (PixelFormat)
        {
        case EPixelFormat::IMAGE_RGB:
        {
            int32_t y = 0;
            size_t ScanlineOffset = 0;
            const int32_t WidthBytes = W * sizeof(uint32_t);
            std::vector<uint8_t> Scanline(WidthBytes);

            while (File.tellg() < static_cast<std::streampos>(ChunkEnd))
            {
                size_t InputBytes = std::min<size_t>(Compressed.size(), ChunkEnd - File.tellg());
                size_t BytesRead = ReadBytes(File, Compressed.data(), InputBytes);
                if (BytesRead == 0)
                {
                    break;
                }

                zstream.next_in = Compressed.data();
                zstream.avail_in = (uInt)BytesRead;

                do {
                    zstream.next_out = Uncompressed.data();
                    zstream.avail_out = (uInt)Uncompressed.size();

                    err = inflate(&zstream, Z_NO_FLUSH);
                    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR)
                    {
                        LOG_ERROR("[{}]\t ReadCompressedImage.", (__FUNCTION__));
                        return false;
                    }

                    size_t UncompressedBytes = Uncompressed.size() - zstream.avail_out;
                    size_t i = 0;
                    while (y < H && UncompressedBytes > 0)
                    {
                        size_t n = std::min<size_t>(UncompressedBytes, Scanline.size() - ScanlineOffset);
                        std::copy(Uncompressed.begin() + i, Uncompressed.begin() + i + n, Scanline.begin() + ScanlineOffset);
                        ScanlineOffset += n;
                        i += n;
                        UncompressedBytes -= n;

                        if (ScanlineOffset == WidthBytes)
                        {
                            const size_t Index = ((Y + y) * Header.Width + X) * sizeof(uint32_t);
                            uint32_t* Address = reinterpret_cast<uint32_t*>(&OutputRGBA[Index]);
                            uint8_t* Buffer = &Scanline[0];
                            for (int x = 0; x < Header.Width; ++x, ++Address)
                            {
                                const uint8_t R = *(Buffer++);
                                const uint8_t G = *(Buffer++);
                                const uint8_t B = *(Buffer++);
                                const uint8_t A = *(Buffer++);
                                *Address = RGBA(R, G, B, A);
                            }
                            ++y;
                            ScanlineOffset = 0;
                        }
                    }
                } while (zstream.avail_in > 0 && zstream.avail_out == 0);
            }
            break;
        }

        case EPixelFormat::IMAGE_GRAYSCALE:
        {
            int32_t y = 0;
            size_t ScanlineOffset = 0;
            const int32_t WidthBytes = W * sizeof(uint16_t);
            std::vector<uint8_t> Scanline(WidthBytes);

            while (File.tellg() < static_cast<std::streampos>(ChunkEnd))
            {
                size_t InputBytes = std::min<size_t>(Compressed.size(), ChunkEnd - File.tellg());
                size_t BytesRead = ReadBytes(File, Compressed.data(), InputBytes);
                if (BytesRead == 0)
                {
                    break;
                }

                zstream.next_in = Compressed.data();
                zstream.avail_in = (uInt)BytesRead;

                do {
                    zstream.next_out = Uncompressed.data();
                    zstream.avail_out = (uInt)Uncompressed.size();

                    err = inflate(&zstream, Z_NO_FLUSH);
                    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR)
                    {
                        LOG_ERROR("[{}]\t ReadCompressedImage.", (__FUNCTION__));
                        return false;
                    }

                    size_t UncompressedBytes = Uncompressed.size() - zstream.avail_out;
                    size_t i = 0;
                    while (y < H && UncompressedBytes > 0)
                    {
                        size_t n = std::min<size_t>(UncompressedBytes, Scanline.size() - ScanlineOffset);
                        std::copy(Uncompressed.begin() + i, Uncompressed.begin() + i + n, Scanline.begin() + ScanlineOffset);
                        ScanlineOffset += n;
                        i += n;
                        UncompressedBytes -= n;

                        if (ScanlineOffset == WidthBytes)
                        {
                            const size_t Index = ((Y + y) * Header.Width + X) * sizeof(uint32_t);
                            uint32_t* Address = reinterpret_cast<uint32_t*>(&OutputRGBA[Index]);
                            uint8_t* Buffer = &Scanline[0];
                            for (int x = 0; x < W; ++x, ++Address)
                            {
                                const uint8_t K = *(Buffer++);
                                const uint8_t A = *(Buffer++);
                                *Address = GrayA(K, A);
                            }
                            ++y;
                            ScanlineOffset = 0;
                        }
                    }
                } while (zstream.avail_in > 0 && zstream.avail_out == 0);
            }
            break;
        }

        case EPixelFormat::IMAGE_INDEXED:
        {
            int32_t y = 0;
            size_t ScanlineOffset = 0;
            const int32_t WidthBytes = W * sizeof(uint8_t);
            std::vector<uint8_t> Scanline(WidthBytes);

            while (File.tellg() < static_cast<std::streampos>(ChunkEnd))
            {
                size_t InputBytes = std::min<size_t>(Compressed.size(), ChunkEnd - File.tellg());
                size_t BytesRead = ReadBytes(File, Compressed.data(), InputBytes);
                if (BytesRead == 0)
                {
                    break;
                }

                zstream.next_in = Compressed.data();
                zstream.avail_in = (uInt)BytesRead;

                do {
                    zstream.next_out = Uncompressed.data();
                    zstream.avail_out = (uInt)Uncompressed.size();

                    err = inflate(&zstream, Z_NO_FLUSH);
                    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR)
                    {
                        LOG_ERROR("[{}]\t ReadCompressedImage.", (__FUNCTION__));
                        return false;
                    }

                    size_t UncompressedBytes = Uncompressed.size() - zstream.avail_out;
                    size_t i = 0;
                    while (y < H && UncompressedBytes > 0)
                    {
                        size_t n = std::min<size_t>(UncompressedBytes, Scanline.size() - ScanlineOffset);
                        std::copy(Uncompressed.begin() + i, Uncompressed.begin() + i + n, Scanline.begin() + ScanlineOffset);
                        ScanlineOffset += n;
                        i += n;
                        UncompressedBytes -= n;

                        if (ScanlineOffset == WidthBytes)
                        {
                            const size_t Index = ((Y + y) * Header.Width + X) * sizeof(uint32_t);
                            uint32_t* Address = reinterpret_cast<uint32_t*>(&OutputRGBA[Index]);
                            uint8_t* Buffer = &Scanline[0];
                            for (int x = 0; x < W; ++x, ++Address)
                            {
                                const uint8_t I = *(Buffer++);
                                *Address = I;
                            }
                            ++y;
                            ScanlineOffset = 0;
                        }
                    }
                } while (zstream.avail_in > 0 && zstream.avail_out == 0);
            }
            break;
        }

        case EPixelFormat::IMAGE_TILEMAP:
        {
            // ToDo:
            return false;
            break;
        }

        }

        return true;
    }

    static bool ReadHeader(std::ifstream& File, FAsepriteHeader& OutputHeader)
    {
        size_t HeaderPos = File.tellg();

        OutputHeader.Size = Read_u32(File);
        OutputHeader.Magic = Read_u16(File);

        // Developers can open any .ase file
#if !defined(ENABLE_DEVMODE)
        if (OutputHeader.Magic != ASE_FILE_MAGIC)
        {
            return false;
        }
#endif

        OutputHeader.Frames             = Read_u16(File);
        OutputHeader.Width              = Read_u16(File);
        OutputHeader.Height             = Read_u16(File);
        OutputHeader.Depth              = Read_u16(File);
        OutputHeader.Flags              = Read_u32(File);
        OutputHeader.Speed              = Read_u16(File);
        OutputHeader.Next               = Read_u32(File);
        OutputHeader.Frit               = Read_u32(File);
        OutputHeader.TransparentIndex   = Read_u8(File);
        OutputHeader.Ignore[0]          = Read_u8(File);
        OutputHeader.Ignore[1]          = Read_u8(File);
        OutputHeader.Ignore[2]          = Read_u8(File);
        OutputHeader.nColors            = Read_u16(File);
        OutputHeader.PixelWidth         = Read_u8(File);
        OutputHeader.PixelHeight        = Read_u8(File);
        OutputHeader.GridX              = (int16_t)Read_u16(File);
        OutputHeader.GridY              = (int16_t)Read_u16(File);
        OutputHeader.GridWidth          = Read_u16(File);
        OutputHeader.GridHeight         = Read_u16(File);

        if (OutputHeader.Depth != 8) // Transparent index only valid for indexed images
        {
            OutputHeader.TransparentIndex = 0;
        }

        if (OutputHeader.nColors == 0) // 0 means 256 (old .ase files)
        {
            OutputHeader.nColors = 256;
        }

        if (OutputHeader.PixelWidth == 0 || OutputHeader.PixelHeight == 0)
        {
            OutputHeader.PixelWidth = 1;
            OutputHeader.PixelHeight = 1;
        }

#if defined(ENABLE_DEVMODE)
        // This is useful to read broken .ase files
        if (OutputHeader.Magic != ASE_FILE_MAGIC)
        {
            OutputHeader.Frames = 256; // Frames number might be not enought for some files
            OutputHeader.Width  = 1024; // Size doesn't matter, the sprite can be crop
            OutputHeader.Height = 1024;
        }
#endif

        File.seekg(HeaderPos + 128, std::ios::beg);
        return true;
    }

    static void ReadFrameHeader(std::ifstream& File, FAsepriteFrameHeader& OutputFrameHeader)
    {
        OutputFrameHeader.Size = Read_u32(File);
        OutputFrameHeader.Magic = Read_u16(File);
        OutputFrameHeader.Chunks = Read_u16(File);
        OutputFrameHeader.Duration = Read_u16(File);
        ReadPadding(File, 2);
        uint32_t nChunks = Read_u32(File);

        if (OutputFrameHeader.Chunks == 0xFFFF && OutputFrameHeader.Chunks < nChunks)
        {
            OutputFrameHeader.Chunks = nChunks;
        }
    }

    static void ReadColorChunk(std::ifstream& File, FPalette& InOutputPalette, bool bScale6bitsTo8bits)
    {
        int32_t Packets = Read_u16(File); // Number of packets

        // Read all packets
        int32_t Skip = 0;
        for (int32_t i = 0; i < Packets; ++i)
        {
            Skip += Read_u8(File);
            int32_t Size = Read_u8(File);
            if (!Size)
            {
                Size = 256;
            }

            for (int32_t c = Skip; c < Skip + Size; ++c)
            {
                const uint8_t R = Read_u8(File);
                const uint8_t G = Read_u8(File);
                const uint8_t B = Read_u8(File);
                InOutputPalette.RGBA[c] = RGBA(bScale6bitsTo8bits ? Scale6bitsTo8bits(R) : R, bScale6bitsTo8bits ? Scale6bitsTo8bits(G) : G, bScale6bitsTo8bits ? Scale6bitsTo8bits(B) : B, 255);
            }
        }
    }

    static void ReadPaletteChunk(std::ifstream& File, FPalette& OutputPalette)
    {
        int32_t NewSize = Read_u32(File);
        int32_t from = Read_u32(File);
        int32_t to = Read_u32(File);
        ReadPadding(File, 8);

        if (NewSize > 0)
            OutputPalette.RGBA.resize(NewSize);

        for (int c = from; c <= to; ++c)
        {
            uint16_t Flags = Read_u16(File);
            uint8_t R = Read_u8(File);
            uint8_t G = Read_u8(File);
            uint8_t B = Read_u8(File);
            uint8_t A = Read_u8(File);
            OutputPalette.RGBA[c] = RGBA(R, G, B, A);

            // Skip name
            if (Flags & ASE_PALETTE_FLAG_HAS_NAME)
            {
                std::string Name = ReadString(File);
                // Ignore color entry name
            }
        }
    }

    static uint16_t ReadCelChunk(
        std::ifstream& File,
        EPixelFormat PixelFormat,
        std::vector<uint8_t>& OutputRGBA,
        const FPalette& Palette,
        const FAsepriteHeader& Header,
        size_t ChunkEnd)
    {
        // Read chunk data
        uint16_t LayerIndex = Read_u16(File);
        int16_t x           = (int16_t)Read_u16(File);
        int16_t y           = (int16_t)Read_u16(File);
        uint8_t Opacity     = Read_u8(File);
        uint16_t CelType    = Read_u16(File);
        int16_t zIndex      = ((int16_t)Read_u16(File));
        ReadPadding(File, 5);

        switch (CelType)
        {
        case ASE_FILE_RAW_CEL:
        {
            // Read width and height
            int16_t w = Read_u16(File);
            int16_t h = Read_u16(File);

            if (w > 0 && h > 0)
            {
                // Read pixel data
                ReadRawImage(File, PixelFormat, OutputRGBA, Header, x, y, w, h);
                if (PixelFormat == EPixelFormat::IMAGE_INDEXED)
                {
                    // convert index to RGBA
                    for (uint16_t _y = 0; _y < Header.Height; ++_y)
                    {
                        for (uint16_t _x = 0; _x < Header.Width; ++_x)
                        {
                            const size_t Index = (_y * Header.Width + _x) * sizeof(uint32_t);
                            uint32_t& Value = reinterpret_cast<uint32_t&>(OutputRGBA[Index]);
                            const uint32_t rgba = Palette.RGBA[Value];
                            Value = rgba;
                        }
                    }
                }
            }
            break;
        }

        case ASE_FILE_LINK_CEL:
        {
            // Read link position
            const uint16_t LinkFrame = Read_u16(File);
            return LinkFrame;
        }

        case ASE_FILE_COMPRESSED_CEL:
        {
            // Read width and height
            int16_t w = Read_u16(File);
            int16_t h = Read_u16(File);

            if (w > 0 && h > 0)
            {
                if (!ReadCompressedImage(File, PixelFormat, OutputRGBA, Header, ChunkEnd, x, y, w, h))
                {
                    LOG_ERROR("[{}]\t ReadCompressedImage.", (__FUNCTION__));
                    break;
                }
                if (PixelFormat == EPixelFormat::IMAGE_INDEXED)
                {
                    // convert index to RGBA
                    for (uint16_t _y = 0; _y < Header.Height; ++_y)
                    {
                        for (uint16_t _x = 0; _x < Header.Width; ++_x)
                        {
                            const size_t Index = (_y * Header.Width + _x) * sizeof(uint32_t);
                            uint32_t& Value = reinterpret_cast<uint32_t&>(OutputRGBA[Index]);

                            uint8_t PaletteIndex = Value & 0xFF;
                            if (Palette.RGBA.size() > PaletteIndex)
                            {
                                const uint32_t rgba = Header.TransparentIndex == PaletteIndex ? 0x00000000 : Palette.RGBA[PaletteIndex];
                                Value = rgba;
                            }
                        }
                    }
                }
            }
            break;
        }

        case ASE_FILE_COMPRESSED_TILEMAP:
        {
            // ToDo:
            //// Read width and height
            //int w = read16();
            //int h = read16();
            //int bitsPerTile = read16();
            //uint32_t tileIDMask = read32();
            //uint32_t tileIDShift = base::mask_shift(tileIDMask);
            //uint32_t xflipMask = read32();
            //uint32_t yflipMask = read32();
            //uint32_t dflipMask = read32();
            //uint32_t flagsMask = (xflipMask | yflipMask | dflipMask);
            //readPadding(10);
            //
            //// We only support 32bpp at the moment
            //// TODO add support for other bpp (8-bit, 16-bpp)
            //if (bitsPerTile != 32) {
            //    delegate()->incompatibilityError(
            //        fmt::format("Unsupported tile format: {0} bits per tile", bitsPerTile));
            //    break;
            //}
            //
            //if (w > 0 && h > 0) {
            //    doc::ImageRef image(doc::Image::create(doc::IMAGE_TILEMAP, w, h));
            //    image->setMaskColor(doc::notile);
            //    image->clear(doc::notile);
            //    read_compressed_image(f(), delegate(), image.get(), header, chunk_end);
            //
            //    // Check if the tileset of this tilemap has the
            //    // "ASE_TILESET_FLAG_ZERO_IS_NOTILE" we have to adjust all
            //    // tile references to the new format (where empty tile is
            //    // zero)
            //    doc::Tileset* ts = static_cast<doc::LayerTilemap*>(layer)->tileset();
            //    doc::tileset_index tsi = static_cast<doc::LayerTilemap*>(layer)->tilesetIndex();
            //    ASSERT(tsi >= 0 && tsi < m_tilesetFlags.size());
            //    if (tsi >= 0 && tsi < m_tilesetFlags.size() &&
            //        (m_tilesetFlags[tsi] & ASE_TILESET_FLAG_ZERO_IS_NOTILE) == 0) {
            //        doc::fix_old_tilemap(image.get(), ts, tileIDMask, flagsMask);
            //    }
            //
            //    // Convert the tile index and masks to a proper in-memory
            //    // representation for the doc-lib.
            //    doc::transform_image<doc::TilemapTraits>(
            //        image.get(),
            //        [ts, tileIDMask, tileIDShift, xflipMask, yflipMask, dflipMask](doc::tile_t tile) {
            //            // Get the tile index.
            //            doc::tile_index ti = ((tile & tileIDMask) >> tileIDShift);
            //
            //            // If the index is out of bounds from the tileset, we
            //            // allow to keep some small values in-memory, but if the
            //            // index is too big, we consider it as a broken file and
            //            // remove the tile (as an huge index bring some lag
            //            // problems in the remove_unused_tiles_from_tileset()
            //            // creating a big Remap structure).
            //            //
            //            // Related to https://github.com/aseprite/aseprite/issues/2877
            //            if (ti > ts->size() && ti > 0xffffff) {
            //                return doc::notile;
            //            }
            //
            //            // Convert read index to doc::tile_i_mask, and flags to doc::tile_f_mask
            //            tile = doc::tile(ti,
            //                ((tile & xflipMask) == xflipMask ? doc::tile_f_xflip : 0) |
            //                ((tile & yflipMask) == yflipMask ? doc::tile_f_yflip : 0) |
            //                ((tile & dflipMask) == dflipMask ? doc::tile_f_dflip : 0));
            //
            //            return tile;
            //        });
            //
            //    cel = std::make_unique<doc::Cel>(frame, image);
            //    cel->setPosition(x, y);
            //    cel->setOpacity(Opacity);
            //    cel->setZIndex(zIndex);
            //}
            LOG_WARNING("[{}]\t Warning: cel type is not parsed {} (skipping).", (__FUNCTION__), CelType);
            break;
        }

        default:
            LOG_ERROR("[{}]\t Unknown cel type found {}.", (__FUNCTION__), CelType);
            break;
        }

        return uint16_t(-1);
    }

    bool Load(const std::filesystem::path& FilePath, FSprite& OutputSprite)
	{
        bool bIgnoreOldColorChunks = false;

        std::ifstream File(FilePath, std::ios::binary);
        if (!File.is_open())
        {
            LOG_ERROR("[{}]\t Cannot open file.", (__FUNCTION__));
            return false;
        }

		FAsepriteHeader Header;
        if (!ReadHeader(File, Header))
        {
            LOG_ERROR("[{}]\t  Error reading header.", (__FUNCTION__));
            return false;
        }

        if (Header.Depth != 32 && Header.Depth != 16 && Header.Depth != 8)
        {
            LOG_ERROR("[{}]\t  Invalid color depth. {}", (__FUNCTION__), Header.Depth);
            return false;
        }

        if (Header.Width < 1 || Header.Height < 1)
        {
            LOG_ERROR("[{}]\t  Invalid sprite size {}x{}", (__FUNCTION__), Header.Width, Header.Height);
            return false;
        }

        OutputSprite.ColorMode = Header.Depth == 32 ? EColorMode::RGB : Header.Depth == 16 ? EColorMode::GRAYSCALE : EColorMode::INDEXED;
        OutputSprite.Width = Header.Width;
        OutputSprite.Height = Header.Height;

        OutputSprite.Frames.resize(Header.Frames);
        {
            OutputSprite.DurationPerFrame.resize(Header.Frames);
            std::fill(OutputSprite.DurationPerFrame.begin(), OutputSprite.DurationPerFrame.end(), std::clamp<uint16_t>(Header.Speed, 1, 65535));
        }

        FPalette Palette;
        {
            Palette.RGBA.resize(Header.nColors);
            switch (OutputSprite.ColorMode)
            {
                // For black and white images
            case EColorMode::GRAYSCALE:
            case EColorMode::BITMAP:
                for (int c = 0; c < Palette.RGBA.size(); c++)
                {
                    int32_t G = 255 * c / int32_t(Palette.RGBA.size() - 1);
                    G = std::clamp(G, 0, 255);
                    Palette.RGBA[c] = RGBA(G, G, G, 255);
                }
                break;
            }
        }

        // Read frame by frame to end-of-file
        for (int Frame = 0; Frame < OutputSprite.Frames.size(); ++Frame)
        {
            const int32_t Size = OutputSprite.ColorMode == EColorMode::INDEXED ? 4/*convert index to RGBA*/ : BytesPerPixelForColormode(OutputSprite.ColorMode);
            OutputSprite.Frames[Frame].resize(OutputSprite.Width * OutputSprite.Height * Size);

            size_t FramePos = File.tellg();
            FAsepriteFrameHeader FrameHeader;
            ReadFrameHeader(File, FrameHeader);

            // Correct frame type
            if (FrameHeader.Magic == ASE_FILE_FRAME_MAGIC)
            {
                // Use frame-duration field?
                if (FrameHeader.Duration > 0)
                {
                    if (Frame >= 0 && Frame < OutputSprite.DurationPerFrame.size())
                    {
                        OutputSprite.DurationPerFrame[Frame] = std::clamp<uint16_t>(FrameHeader.Duration, 1, 65535);
                    }
                }

                // Read chunks
                for (uint32_t c = 0; c < FrameHeader.Chunks; c++)
                {
                    // Start chunk position
                    size_t ChunkPos = File.tellg();

                    // Read chunk information
                    int32_t ChunkSize = Read_u32(File);
                    uint16_t ChunkType = Read_u16(File);

                    switch (ChunkType)
                    {
                    case ASE_FILE_CHUNK_FLI_COLOR:
                    case ASE_FILE_CHUNK_FLI_COLOR2:
                    {
                        if (!bIgnoreOldColorChunks)
                        {
                            ReadColorChunk(File, Palette, ChunkType == ASE_FILE_CHUNK_FLI_COLOR);
                        }
                        break;
                    }

                    case ASE_FILE_CHUNK_PALETTE:
                    {
                        ReadPaletteChunk(File, Palette);
                        bIgnoreOldColorChunks = true;
                        break;
                    }

                    case ASE_FILE_CHUNK_LAYER:
                    {
                        // ToDo:
                        //doc::Layer* newLayer =
                        //    readLayerChunk(&header, sprite.get(), &last_layer, &current_level);
                        //if (newLayer) {
                        //    m_allLayers.push_back(newLayer);
                        //    last_object_with_user_data = newLayer;
                        //}
                        //else {
                        //    // Add a null layer only to match the "layer index" in cel chunk
                        //    m_allLayers.push_back(nullptr);
                        //    last_object_with_user_data = nullptr;
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_CEL:
                    {
                        const uint16_t LinkFrame = ReadCelChunk(
                            File,
                            PixelFormat(OutputSprite.ColorMode),
                            OutputSprite.Frames[Frame],
                            Palette,
                            Header,
                            ChunkPos + ChunkSize);
                        if (LinkFrame != uint16_t(-1))
                        {
                            memcpy(OutputSprite.Frames[Frame].data(), OutputSprite.Frames[LinkFrame].data(), Size);
                        }

                        break;
                    }

                    case ASE_FILE_CHUNK_CEL_EXTRA:
                    {
                        // ToDo:
                        //if (last_cel)
                        //{
                        //    readCelExtraChunk(last_cel);
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_COLOR_PROFILE:
                    {
                        // ToDo:
                        //readColorProfile(sprite.get());
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_EXTERNAL_FILE:
                    {
                        // ToDo:
                        //readExternalFiles(extFiles);
                        //
                        //// Tile management plugin
                        //if (!extFiles.empty())
                        //{
                        //    std::string fn = extFiles.tileManagementPlugin();
                        //    if (!fn.empty())
                        //    {
                        //        sprite->setTileManagementPlugin(fn);
                        //    }
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_MASK:
                    {
                        // ToDo:
                        //doc::Mask* mask = readMaskChunk();
                        //if (mask)
                        //{
                        //    delete mask; // TODO add the mask in some place?
                        //}
                        //else
                        //{
                        //    delegate()->error("Warning: Cannot load a mask chunk");
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_PATH:
                    {
                        // Ignore
                        break;
                    }

                    case ASE_FILE_CHUNK_TAGS:
                    {
                        // ToDo:
                        //readTagsChunk(&sprite->tags());
                        //tag_it = sprite->tags().begin();
                        //tag_end = sprite->tags().end();
                        //
                        //if (tag_it != tag_end)
                        //{
                        //    last_object_with_user_data = *tag_it;
                        //}
                        //else
                        //{
                        //    last_object_with_user_data = nullptr;
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_SLICES:
                    {
                        // ToDo:
                        // readSlicesChunk(sprite->slices());
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_SLICE:
                    {
                        // ToDo:
                        //doc::Slice* slice = readSliceChunk(sprite->slices());
                        //if (slice)
                        //{
                        //    last_object_with_user_data = slice;
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_USER_DATA:
                    {
                        // ToDo:
                        //doc::UserData userData;
                        //readUserDataChunk(&userData, extFiles);
                        //
                        //if (last_object_with_user_data)
                        // {
                        //    last_object_with_user_data->setUserData(userData);
                        //
                        //    switch (last_object_with_user_data->type()) {
                        //    case doc::ObjectType::Tag:
                        //        // Tags are a special case, user data for tags come
                        //        // all together (one next to other) after the tags
                        //        // chunk, in the same order:
                        //        //
                        //        // * TAGS CHUNK (TAG1, TAG2, ..., TAGn)
                        //        // * USER DATA CHUNK FOR TAG1
                        //        // * USER DATA CHUNK FOR TAG2
                        //        // * ...
                        //        // * USER DATA CHUNK FOR TAGn
                        //        //
                        //        // So here we expect that the next user data chunk
                        //        // will correspond to the next tag in the tags
                        //        // collection.
                        //        ++tag_it;
                        //
                        //        if (tag_it != tag_end)
                        //            last_object_with_user_data = *tag_it;
                        //        else
                        //            last_object_with_user_data = nullptr;
                        //        break;
                        //    case doc::ObjectType::Tileset:
                        //        // Read tiles user datas.
                        //        // TODO: Should we refactor how tile user data is handled so we can actually
                        //        // decode this user data chunks the same way as the user data chunks for
                        //        // the tags?
                        //        doc::Tileset* tileset = static_cast<doc::Tileset*>(last_object_with_user_data);
                        //        readTilesData(tileset, extFiles);
                        //        last_object_with_user_data = nullptr;
                        //        break;
                        //    }
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    case ASE_FILE_CHUNK_TILESET:
                    {
                        // ToDo:
                        //doc::Tileset* tileset = readTilesetChunk(sprite.get(), &header, extFiles);
                        //if (tileset)
                        //{
                        //    last_object_with_user_data = tileset;
                        //}
                        LOG_WARNING("[{}]\t Warning: chunk type is not parsed {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }

                    default:
                        LOG_WARNING("[{}]\t Warning: Unsupported chunk type {} (skipping).", (__FUNCTION__), ChunkType);
                        break;
                    }
                    
                    // Skip chunk size
                    File.seekg(ChunkPos + ChunkSize);
                }
            }

            // Skip frame size
            File.seekg(FramePos + FrameHeader.Size);
        }

        OutputSprite.TransparentColor = Palette.RGBA[Header.TransparentIndex];

        return true;
	}
}