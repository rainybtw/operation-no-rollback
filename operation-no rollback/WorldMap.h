#pragma once

#include "Continent.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <filesystem>

// The world map: the picture players see, plus a hidden mask that says which
// continent every pixel belongs to.
//
// A shader reads each pixel's continent from the mask and looks up that
// continent's color in a tiny palette. Recoloring a continent therefore means
// changing one palette entry, with no extra drawing.
class WorldMap
{
public:
    WorldMap();
    WorldMap(const WorldMap&) = delete;
    WorldMap& operator=(const WorldMap&) = delete;

    bool load(const std::filesystem::path& pictureFile, const std::filesystem::path& maskFile);

    sf::Vector2f size() const;
    Continent continentAt(sf::Vector2f worldPoint) const;

    void clearTints();
    void setTint(Continent continent, sf::Color color);

    void draw(sf::RenderTarget& target);

private:
    bool loadShader();

    sf::Texture pictureTexture_;
    sf::Image maskImage_;       // read by the CPU to answer "which continent is here?"
    sf::Texture maskTexture_;   // read by the shader on the graphics card
    sf::Image paletteImage_;
    sf::Texture paletteTexture_;
    sf::Shader tintShader_;
};
