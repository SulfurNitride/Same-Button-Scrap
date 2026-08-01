#include "PCH.h"

#include "Settings.h"

#include <REX/W32/KERNEL32.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    Settings::Config config;

    std::string Trim(std::string value)
    {
        const auto isNotSpace = [](const unsigned char character) {
            return !std::isspace(character);
        };
        value.erase(
            value.begin(),
            std::find_if(value.begin(), value.end(), isNotSpace));
        value.erase(
            std::find_if(value.rbegin(), value.rend(), isNotSpace).base(),
            value.end());
        return value;
    }

    std::string Lower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    std::filesystem::path GetSettingsPath()
    {
        constexpr std::uint32_t kModulePathCapacity{ 260 };
        wchar_t modulePath[kModulePathCapacity]{};
        REX::W32::GetModuleFileNameW(
            REX::W32::GetCurrentModule(),
            modulePath,
            kModulePathCapacity);
        return std::filesystem::path(modulePath)
            .replace_filename(L"ScrapWithSameButton.ini");
    }

    bool ParseBool(const std::string& value, const bool fallback)
    {
        const auto normalized = Lower(Trim(value));
        if (normalized == "true" || normalized == "yes" ||
            normalized == "on" || normalized == "1") {
            return true;
        }
        if (normalized == "false" || normalized == "no" ||
            normalized == "off" || normalized == "0") {
            return false;
        }
        return fallback;
    }
}

const Settings::Config& Settings::Get()
{
    return config;
}

void Settings::Load()
{
    config = {};
    const auto path = GetSettingsPath();
    std::ifstream file(path);
    if (!file) {
        spdlog::info(
            "No settings file found at {}; using SameButton mode without "
            "material notifications",
            path.string());
        return;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        if (section != "scrapping") {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const auto key = Lower(Trim(line.substr(0, separator)));
        const auto value = Trim(line.substr(separator + 1));
        if (key == "mode") {
            const auto mode = Lower(value);
            if (mode == "instant") {
                config.mode = ScrapMode::kInstant;
            } else if (mode == "samebutton") {
                config.mode = ScrapMode::kSameButton;
            } else {
                spdlog::warn(
                    "Unknown scrap Mode '{}'; using SameButton",
                    value);
            }
        } else if (key == "showmaterials") {
            config.showMaterials = ParseBool(value, config.showMaterials);
        }
    }

    spdlog::info(
        "Settings: Mode={}, ShowMaterials={}",
        config.mode == ScrapMode::kInstant ? "Instant" : "SameButton",
        config.showMaterials);
}
