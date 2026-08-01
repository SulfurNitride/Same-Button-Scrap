#pragma once

namespace Settings
{
    enum class ScrapMode
    {
        kSameButton,
        kInstant
    };

    struct Config
    {
        ScrapMode mode{ ScrapMode::kSameButton };
        bool showMaterials{ false };
    };

    const Config& Get();
    void Load();
}
