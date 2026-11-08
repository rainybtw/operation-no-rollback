#include "Camera.h"
#include "MapController.h"
#include "WorldMap.h"
 
#include <SFML/Graphics.hpp>
 
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
 
namespace
{
    constexpr unsigned kWindowWidth = 1280;
    constexpr unsigned kWindowHeight = 720;
    constexpr unsigned kFramesPerSecond = 60;
    const char* const kWindowTitle = "No Rollback";
 
    const char* const kPictureFile = "assets/world_base_cool.png";
    const char* const kMaskFile = "assets/continent_mask.png";
    const char* const kFontFile = "C:/Windows/Fonts/consola.ttf";   // placeholder until we pick a font
 
    const sf::Color kBackgroundColor(5, 9, 19);
    const sf::Color kLabelColor(223, 233, 245);
 
    // Orange-red is reserved for hacked regions, so highlights stay neutral white and blue.
    const sf::Color kHoverTint(255, 255, 255, 28);
    const sf::Color kSelectedTint(150, 200, 255, 70);
 
    constexpr unsigned kLabelSize = 22;
    const sf::Vector2f kLabelPosition(16.f, 12.f);
 
    void processEvents(sf::RenderWindow& window, Camera& camera, MapController& controller)
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                camera.resizeWindow(resized->size);
            }
 
            controller.handleEvent(*event);
        }
    }
 
    void applyHighlights(WorldMap& map, const MapController& controller)
    {
        map.clearTints();
        map.setTint(controller.hovered(), kHoverTint);
        map.setTint(controller.selected(), kSelectedTint);   // drawn last, so it wins over hover
    }
 
    void drawFrame(sf::RenderWindow& window, WorldMap& map, const Camera& camera, const sf::Text& label)
    {
        window.clear(kBackgroundColor);
 
        window.setView(sf::View(camera.center(), camera.viewSize()));
        map.draw(window);
 
        window.setView(sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(window.getSize()))));
        window.draw(label);
 
        window.display();
    }
}
 
int main()
{
    sf::RenderWindow window(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle);
    window.setFramerateLimit(kFramesPerSecond);
 
    WorldMap map;
    if (!map.load(kPictureFile, kMaskFile))
    {
        return EXIT_FAILURE;
    }
 
    sf::Font font;
    if (!font.openFromFile(kFontFile))
    {
        std::cerr << "Could not load the font " << kFontFile << "\n";
        return EXIT_FAILURE;
    }
 
    Camera camera(map.size(), window.getSize());
    MapController controller(camera, map);
 
    sf::Text label(font, "", kLabelSize);
    label.setFillColor(kLabelColor);
    label.setPosition(kLabelPosition);
 
    while (window.isOpen())
    {
        processEvents(window, camera, controller);
        controller.update();
        applyHighlights(map, controller);
        label.setString(std::string(continentName(controller.hovered())));
        drawFrame(window, map, camera, label);
    }
 
    return EXIT_SUCCESS;
}
