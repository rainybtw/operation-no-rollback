#pragma once

#include <SFML/System/Vector2.hpp>

// Decides which part of the world is visible. It knows nothing about drawing,
// so its rules can be tested on their own.
//
// Zoom 1 shows the whole map. Zooming in shrinks the visible area. The visible
// area never leaves the map, which keeps panning and zooming predictable.
class Camera
{
public:
    static constexpr float kMinZoom = 1.f;
    static constexpr float kMaxZoom = 4.f;

    Camera(sf::Vector2f worldSize, sf::Vector2u windowSize);

    void resizeWindow(sf::Vector2u windowSize);

    // factor > 1 zooms in, factor < 1 zooms out. The world point under the cursor stays under the cursor.
    void zoomAt(sf::Vector2i screenPoint, float factor);

    // Moves the map so the point that was under previousScreenPoint is now under currentScreenPoint.
    void dragBy(sf::Vector2i previousScreenPoint, sf::Vector2i currentScreenPoint);

    sf::Vector2f screenToWorld(sf::Vector2i screenPoint) const;
    sf::Vector2f center() const;
    sf::Vector2f viewSize() const;

private:
    float worldUnitsPerPixelAtMinZoom() const;
    void keepViewInsideWorld();

    sf::Vector2f worldSize_;
    sf::Vector2f windowSize_;
    sf::Vector2f center_;
    float zoom_ = kMinZoom;
};
