#include "PCH.h"

#include "Hooks.h"

namespace
{
    constexpr std::string_view kPluginName{ "ScrapWithSameButton" };

    consteval F4SE::PluginVersionData MakeVersionData()
    {
        F4SE::PluginVersionData data{};
        data.PluginVersion({
            SWSB_VERSION_MAJOR,
            SWSB_VERSION_MINOR,
            SWSB_VERSION_PATCH,
            0
        });
        data.PluginName(kPluginName);
        data.AuthorName("Scrap With Same Button contributors");
        data.UsesAddressLibrary(true);
        data.UsesSigScanning(false);
        data.IsLayoutDependent(true);
        data.HasNoStructUse(false);
#if defined(SWSB_RUNTIME_OG)
        data.CompatibleVersions({
            F4SE::RUNTIME_1_10_163
        });
#elif defined(SWSB_RUNTIME_NG)
        data.CompatibleVersions({
            F4SE::RUNTIME_1_10_980,
            F4SE::RUNTIME_1_10_984
        });
#elif defined(SWSB_RUNTIME_AE)
        data.CompatibleVersions({
            F4SE::RUNTIME_1_11_137,
            F4SE::RUNTIME_1_11_159,
            F4SE::RUNTIME_1_11_169,
            F4SE::RUNTIME_1_11_191,
            F4SE::RUNTIME_1_11_221
        });
#else
#error "A Scrap With Same Button runtime variant must be selected"
#endif
        return data;
    }
}

F4SE_PLUGIN_VERSION = MakeVersionData();

F4SE_EXPORT bool F4SEPlugin_Load(const F4SE::LoadInterface* f4se)
{
    F4SE::Init(f4se, {
        .log = true,
        .logName = kPluginName.data(),
        .hook = true
    });

    spdlog::info(
        "{} {} loading for Fallout 4 {}",
        kPluginName,
        SWSB_VERSION,
        REX::FModule::GetExecutingModule().GetFileVersion().string());

    if (!Hooks::Install()) {
        spdlog::critical("Failed to install scrap confirmation hooks");
        return false;
    }
    return true;
}
