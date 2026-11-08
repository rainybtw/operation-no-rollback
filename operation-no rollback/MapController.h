#pragma once

#include "Continent.h"

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

class Camera;
class WorldMap;

// Turns mouse input into camera movement and continent selection.
//
//   mouse wheel                 zoom toward the cursor
//   drag (left, right, middle)  pan the map
//   left click without moving   select or deselect the continent under the cursor
class MapController
{
public:
    MapController(Camera& camera, const WorldMap& map);

    void handleEvent(const sf::Event& event);
    void update();

    Continent hovered() const;
    Continent selected() const;

private:
    enum class PointerMode
    {
        Idle,
        LeftPressed,   // may still turn out to be a click or a drag
        Panning
    };

    void onWheelScrolled(const sf::Event::MouseWheelScrolled& wheel);
    void onMousePressed(const sf::Event::MouseButtonPressed& pressed);
    void onMouseReleased(const sf::Event::MouseButtonReleased& released);
    void onMouseMoved(const sf::Event::MouseMoved& moved);

    Continent continentUnderMouse() const;
    void toggleSelection(Continent clicked);

    Camera& camera_;
    const WorldMap& map_;

    PointerMode mode_ = PointerMode::Idle;
    sf::Vector2i mousePosition_;
    sf::Vector2i pressPosition_;
    Continent hovered_ = Continent::None;
    Continent selected_ = Continent::None;
};
