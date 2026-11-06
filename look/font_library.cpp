#include "look/font_library.hpp"
#include "look/embedded_assets.hpp"

#include "onyx/font.hpp"
#include "onyx/resources.hpp"

#include "tkit/utils/logging.hpp"

namespace Lim
{
Onyx::Resource FontLibrary::Get()
{
    if (m_Font)
        return *m_Font;

    const Embedded::EmbeddedAsset &asset = Embedded::FiraCodeNerdFontMonoPlusMedium();

    Onyx::FontLoadOptions options{};
    options.CharSet.LoadBlock(Onyx::UnicodeBlocks.ASCII);
    options.CharSet.Load(Onyx::CodePoint_ArrowDown);
    options.CharSet.Load(Onyx::CodePoint_ArrowRight);
    options.CharSet.Load(Onyx::CodePoint_Bullet);
    options.CharSet.Load(Onyx::CodePoint_Cross);

    Onyx::Result<Onyx::FontData> loaded =
        Onyx::Font_LoadDataFromMemory(asset.Bytes.GetData(), static_cast<u32>(asset.Bytes.GetSize()), options);
    if (!loaded)
    {
        TKIT_LOG_ERROR("[LIM][FONTS] Could not load '{}' ({}), using the built-in font", asset.Name,
                       loaded.GetError().GetMessage());
        m_Font = Onyx::Resources_GetDefault().Font;
        return *m_Font;
    }

    const Onyx::ResourcePool pool = Onyx::Resources_GetDefault().FontPool;
    m_Font = Onyx::FontPool_RegisterFont(pool, *loaded);
    Onyx::Font_UnloadData(*loaded);
    Onyx::Resources_Sync(Onyx::SyncFlag_Fonts);
    return *m_Font;
}
} // namespace Lim
