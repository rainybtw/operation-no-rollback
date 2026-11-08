#include "Camera.h"

#include <algorithm>

namespace
{
    // When the view is longer than the world on this axis, the world is simply centered.
    float clampCenterOnAxis(float center, float viewLength, float worldLength)
    {
        if (viewLength >= worldLength)
        {
            return worldLength / 2.f;
        }

        const float halfView = viewLength / 2.f;
        return std::clamp(center, halfView, worldLength - halfView);
    }
}

Camera::Camera(sf::Vector2f worldSize, sf::Vector2u windowSize)
    : worldSize_(worldSize)
    , windowSize_(windowSize)
    , center_(worldSize / 2.f)
{
}

void Camera::resizeWindow(sf::Vector2u windowSize)
{
    const bool isMinimized = windowSize.x == 0 || windowSize.y == 0;
    if (isMinimized)
    {
        return;
    }

    windowSize_ = sf::Vector2f(windowSize);
    keepViewInsideWorld();
}

void Camera::zoomAt(sf::Vector2i screenPoint, float factor)
{
    const sf::Vector2f worldPointBefore = screenToWorld(screenPoint);

    zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);

    // The zoom changed what is under the cursor, so shift the camera to put the old point back.
    const sf::Vector2f worldPointAfter = screenToWorld(screenPoint);
    center_ += worldPointBefore - worldPointAfter;
    keepViewInsideWorld();
}

void Camera::dragBy(sf::Vector2i previousScreenPoint, sf::Vector2i currentScreenPoint)
{
    center_ += screenToWorld(previousScreenPoint) - screenToWorld(currentScreenPoint);
    keepViewInsideWorld();
}

sf::Vector2f Camera::screenToWorld(sf::Vector2i screenPoint) const
{
    const sf::Vector2f screen(screenPoint);
    const sf::Vector2f size = viewSize();
    const float fractionAcrossWindow = screen.x / windowSize_.x;
    const float fractionDownWindow = screen.y / windowSize_.y;

    return sf::Vector2f(center_.x + (fractionAcrossWindow - 0.5f) * size.x,
                        center_.y + (fractionDownWindow - 0.5f) * size.y);
}

sf::Vector2f Camera::center() const
{
    return center_;
}

sf::Vector2f Camera::viewSize() const
{
    return windowSize_ * (worldUnitsPerPixelAtMinZoom() / zoom_);
}

// How many world units one screen pixel covers when the whole map is visible.
float Camera::worldUnitsPerPixelAtMinZoom() const
{
    return std::max(worldSize_.x / windowSize_.x, worldSize_.y / windowSize_.y);
}

void Camera::keepViewInsideWorld()
{
    const sf::Vector2f size = viewSize();
    center_.x = clampCenterOnAxis(center_.x, size.x, worldSize_.x);
    center_.y = clampCenterOnAxis(center_.y, size.y, worldSize_.y);
}
