#include "lim/font_library.hpp"
#include "lim/embedded_assets.hpp"

#include "onyx/font.hpp"
#include "onyx/resources.hpp"

#include "tkit/utils/logging.hpp"

namespace Lim
{
Onyx::Resource FontLibrary::Get()
{
    if (m_Font)
        return *m_Font;

    const Embedded::EmbeddedAsset &asset = Embedded::FiraCodeNerdFontMonoRegular();
    Onyx::Result<Onyx::FontData> loaded = Onyx::LoadFontDataFromMemory(
        asset.Bytes.GetData(), static_cast<u32>(asset.Bytes.GetSize()), Onyx::FontLoadOptions{});
    if (!loaded)
    {
        TKIT_LOG_ERROR("[LIM][FONTS] Could not load '{}' ({}), using the built-in font", asset.Name,
                       loaded.GetError().GetMessage());
        m_Font = Onyx::Resources::GetDefaultResources().Font;
        return *m_Font;
    }

    const Onyx::ResourcePool pool = Onyx::Resources::GetDefaultResources().FontPool;
    m_Font = Onyx::Resources::RegisterFont(pool, *loaded);
    Onyx::UnloadFontData(*loaded);
    Onyx::Resources::Sync(Onyx::SyncFlag_Fonts);
    return *m_Font;
}
} // namespace Lim
