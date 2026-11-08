#pragma once

#include <cstdint>
#include <string_view>

// Each value is the number stored in the red channel of continent_mask.png.
enum class Continent : std::uint8_t
{
    None = 0,
    NorthAmerica,
    SouthAmerica,
    Europe,
    Africa,
    Asia,
    Oceania
};

inline std::string_view continentName(Continent continent)
{
    switch (continent)
    {
        case Continent::NorthAmerica: return "North America";
        case Continent::SouthAmerica: return "South America";
        case Continent::Europe:       return "Europe";
        case Continent::Africa:       return "Africa";
        case Continent::Asia:         return "Asia";
        case Continent::Oceania:      return "Oceania";
        case Continent::None:         break;
    }

    return "";
}
