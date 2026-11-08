#include "WorldMap.h"

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <iostream>

namespace
{
    constexpr unsigned kPaletteSize = 8;
    static_assert(kPaletteSize > static_cast<unsigned>(Continent::Oceania),
                  "The palette needs one slot for every continent");

    const char* const kTintShaderSource = R"(
        uniform sampler2D mask;
        uniform sampler2D palette;
        uniform float paletteSize;

        void main()
        {
            float continentId = texture2D(mask, gl_TexCoord[0].xy).r * 255.0;
            vec2 paletteCoordinate = vec2((continentId + 0.5) / paletteSize, 0.5);
            gl_FragColor = texture2D(palette, paletteCoordinate);
        }
    )";

    void reportLoadFailure(const std::filesystem::path& file)
    {
        std::cerr << "Could not load " << file.string() << ". Check that the file exists "
                  << "and that the graphics card supports textures this large "
                  << "(this one allows " << sf::Texture::getMaximumSize() << " pixels).\n";
    }
}

WorldMap::WorldMap()
    : paletteImage_(sf::Vector2u(kPaletteSize, 1u), sf::Color::Transparent)
{
}

bool WorldMap::load(const std::filesystem::path& pictureFile, const std::filesystem::path& maskFile)
{
    if (!pictureTexture_.loadFromFile(pictureFile))
    {
        reportLoadFailure(pictureFile);
        return false;
    }

    if (!maskImage_.loadFromFile(maskFile) || !maskTexture_.loadFromImage(maskImage_))
    {
        reportLoadFailure(maskFile);
        return false;
    }

    if (!paletteTexture_.resize(sf::Vector2u(kPaletteSize, 1u)))
    {
        std::cerr << "Could not create the palette texture.\n";
        return false;
    }

    // Smoothing plus mipmaps keeps the picture clean when it is drawn smaller than its real size.
    pictureTexture_.setSmooth(true);
    if (!pictureTexture_.generateMipmap())
    {
        std::cerr << "Warning: mipmaps could not be generated, the zoomed-out map may look noisy.\n";
    }

    return loadShader();
}

sf::Vector2f WorldMap::size() const
{
    return sf::Vector2f(pictureTexture_.getSize());
}

Continent WorldMap::continentAt(sf::Vector2f worldPoint) const
{
    const sf::FloatRect mapBounds(sf::Vector2f(0.f, 0.f), size());
    if (!mapBounds.contains(worldPoint))
    {
        return Continent::None;
    }

    const sf::Vector2u pixel(static_cast<unsigned>(worldPoint.x), static_cast<unsigned>(worldPoint.y));
    return static_cast<Continent>(maskImage_.getPixel(pixel).r);
}

void WorldMap::clearTints()
{
    for (unsigned slot = 0; slot < kPaletteSize; ++slot)
    {
        paletteImage_.setPixel(sf::Vector2u(slot, 0u), sf::Color::Transparent);
    }
}

void WorldMap::setTint(Continent continent, sf::Color color)
{
    if (continent == Continent::None)
    {
        return;
    }

    paletteImage_.setPixel(sf::Vector2u(static_cast<unsigned>(continent), 0u), color);
}

void WorldMap::draw(sf::RenderTarget& target)
{
    paletteTexture_.update(paletteImage_);

    target.draw(sf::Sprite(pictureTexture_));

    sf::RenderStates tintedStates;
    tintedStates.shader = &tintShader_;
    target.draw(sf::Sprite(maskTexture_), tintedStates);
}

bool WorldMap::loadShader()
{
    if (!tintShader_.loadFromMemory(kTintShaderSource, sf::Shader::Type::Fragment))
    {
        std::cerr << "Could not compile the continent tint shader.\n";
        return false;
    }

    tintShader_.setUniform("mask", sf::Shader::CurrentTexture);
    tintShader_.setUniform("palette", paletteTexture_);
    tintShader_.setUniform("paletteSize", static_cast<float>(kPaletteSize));
    return true;
}
