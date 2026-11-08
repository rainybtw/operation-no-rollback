#include "MapController.h"

#include "Camera.h"
#include "WorldMap.h"

#include <cmath>

namespace
{
    constexpr float kZoomStep = 1.15f;
    constexpr float kDragThresholdPixels = 5.f;

    float distanceBetween(sf::Vector2i first, sf::Vector2i second)
    {
        const float dx = static_cast<float>(second.x - first.x);
        const float dy = static_cast<float>(second.y - first.y);
        return std::hypot(dx, dy);
    }
}

MapController::MapController(Camera& camera, const WorldMap& map)
    : camera_(camera)
    , map_(map)
{
}

void MapController::handleEvent(const sf::Event& event)
{
    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        onWheelScrolled(*wheel);
    }
    else if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        onMousePressed(*pressed);
    }
    else if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        onMouseReleased(*released);
    }
    else if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
    {
        onMouseMoved(*moved);
    }
}

void MapController::update()
{
    hovered_ = continentUnderMouse();
}

Continent MapController::hovered() const
{
    return hovered_;
}

Continent MapController::selected() const
{
    return selected_;
}

void MapController::onWheelScrolled(const sf::Event::MouseWheelScrolled& wheel)
{
    const float factor = wheel.delta > 0.f ? kZoomStep : 1.f / kZoomStep;
    camera_.zoomAt(wheel.position, factor);
}

void MapController::onMousePressed(const sf::Event::MouseButtonPressed& pressed)
{
    mousePosition_ = pressed.position;

    if (pressed.button == sf::Mouse::Button::Left)
    {
        mode_ = PointerMode::LeftPressed;
        pressPosition_ = pressed.position;
    }
    else if (pressed.button == sf::Mouse::Button::Right || pressed.button == sf::Mouse::Button::Middle)
    {
        mode_ = PointerMode::Panning;
    }
}

void MapController::onMouseReleased(const sf::Event::MouseButtonReleased& released)
{
    const bool wasClick = mode_ == PointerMode::LeftPressed && released.button == sf::Mouse::Button::Left;
    if (wasClick)
    {
        toggleSelection(continentUnderMouse());
    }

    mode_ = PointerMode::Idle;
}

void MapController::onMouseMoved(const sf::Event::MouseMoved& moved)
{
    const sf::Vector2i previousPosition = mousePosition_;
    mousePosition_ = moved.position;

    const bool movedFarEnoughToPan = mode_ == PointerMode::LeftPressed
        && distanceBetween(pressPosition_, mousePosition_) >= kDragThresholdPixels;
    if (movedFarEnoughToPan)
    {
        mode_ = PointerMode::Panning;
    }

    if (mode_ == PointerMode::Panning)
    {
        camera_.dragBy(previousPosition, mousePosition_);
    }
}

Continent MapController::continentUnderMouse() const
{
    return map_.continentAt(camera_.screenToWorld(mousePosition_));
}

void MapController::toggleSelection(Continent clicked)
{
    selected_ = (selected_ == clicked) ? Continent::None : clicked;
}
