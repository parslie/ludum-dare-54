#include "../../headers/game/game.h"

#include <map>
#include <vector>

#include "../../headers/game_object.h"
#include "../../headers/scene.h"
#include "../../headers/utils/constants.h"

#include "../../headers/game/room_generator.h"

static Vector2D getMousePos()
{
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    return {(float)mouseX, (float)mouseY};
}

namespace game
{
    Game::Game(SDL_Renderer *renderer)
        : renderer(renderer)
        , cursor(Cursor("Cursor"))
        , fpsText({"", 0, SCREEN_HEIGHT - 28})
    {
        std::map<std::string, FurnitureMeta> furniture {
            {"bed",  			{1 , 12, {"bedroom"}}}, 
            {"sofa", 			{1 , 10, {"livingroom"}}}, 
            {"piano",			{1 , 20, {"bedroom", "livingroom"}}}, 
            {"plant",			{21, 3 , {"bedroom", "livingroom", "bathroom"}}},
            {"oven", 			{1 , 15, {"kitchen"}}},
            {"bathtub", 		{1 , 10, {"bathroom"}}}, 
            {"bedsidetable",	{2 , 6 , {"bedroom"}}},
            {"bookshelf", 		{2 , 7 , {"bedroom", "livingroom"}}}, 
            {"chair", 			{12, 5 , {"bedroom", "livingroom", "kitchen"}}},
            {"coffeetable", 	{2 , 7 , {"bedroom", "livingroom", "kitchen"}}},
            {"dinnertable", 	{2 , 10, {"livingroom", "kitchen"}}},
            {"dishwasher", 		{1 , 13, {"kitchen"}}},
            {"dresser", 		{2 , 9 , {"bedroom", "livingroom"}}},
            {"fridge", 			{1 , 14, {"kitchen"}}}, 
            {"lamp", 			{14, 3 , {"bedroom", "livingroom", "kitchen", "bathroom"}}}, 
            {"sink", 			{2 , 12, {"kitchen", "bathroom"}}}, 
            {"toilet", 			{1 , 7 , {"bathroom"}}}, 
            {"washingmachine", 	{1 , 13, {"bathroom"}}},
            {"washingstation", 	{1 , 7 , {"bathroom"}}}
        };

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<int> xPositionDistribution(240, SCREEN_WIDTH-240);
        std::uniform_int_distribution<int> yPositionDistribution(0, 160);
        std::uniform_int_distribution<int> velocityDistribution(-10, 10);

        for (auto const& [key, val] : furniture)
        {
            std::uniform_int_distribution<int> amountDistribution(1, val.maxAmount);
            int amount = amountDistribution(generator);

            for(int i = 0; i < amount; i++)
            {
                Furniture* furniturePtr = new Furniture {
                    key,
                    "./resources/furniture/" + key + ".png", 
                    val.weight
                };
                furniturePtr->loadTexture(renderer);

                boxes.push_back(new Box(
                    {(float)xPositionDistribution(generator), (float)yPositionDistribution(generator)}, 
                    "Box", 
                    "./resources/box.png",
                    furniturePtr));
            }
        }

        // Living Room, Dining Room, Pantry, Kitchen, Laundry, Bedroom, Bathroom
        RoomGenerator roomGenerator{4};
        roomScene = roomGenerator.generateRoomScene();

        harold = new Harold({64, 64}); // TODO: change position
    }

    Game::~Game()
    {
        free(harold);

        for (auto box : boxes)
            free(box);
        if (currFurn != nullptr)
            free(currFurn);
        for (auto furn : placedFurn)
            free(furn);
        
        free(roomScene);
    }

    bool Game::loadMedia(Canvas &canvas)
    {
        bool success = true;

        background.getTexture().loadFromFile("./resources/grass.png", renderer);
        floor.getTexture().loadFromFile("./resources/floor.png", renderer);

        fpsText.loadFont("./resources/font.ttf", 28);
        fpsText.loadTexture(renderer);
        canvas.addObj(&fpsText);

        cursor.loadTexture(renderer);

        for (auto box : boxes)
        {
            box->loadTexture(renderer);
            box->furniture->loadTexture(renderer);
        }

        for (auto* wall : roomScene->getObjs())
        {
            wall->loadTexture(renderer);
        }

        return success;
    }

    void Game::render()
    {
        background.render(renderer);
        floor.render(renderer);

        for (auto box : boxes)
            box->render(renderer);
        if (currFurn != nullptr)
            currFurn->render(renderer);
        for (auto furniture : placedFurn)
            furniture->render(renderer);

        harold->updateTexture(renderer);
        harold->render(renderer);

        roomScene->render(renderer);

        fpsText.render(renderer);

        cursor.updateTexture(renderer);
        cursor.render(renderer);
    }

    void Game::placeFurn()
    {
        if (currFurn)
        {
            placedFurn.push_back(currFurn);
            currFurn = nullptr;
        }
    }

    void Game::handleEvent(SDL_Event *event)
    {
        Box *clickedBox = nullptr;

        for (auto box : boxes)
        {
            box->handleEvent(event);

            switch (box->getCurrentState())
            {
            case State::MOUSE_OVER_MOTION:
                cursor.isHovering = true;
                break;
            case State::MOUSE_OUT:
                cursor.isHovering = false;
                break;
            case State::MOUSE_DOWN:
                if (!currFurn)
                    clickedBox = box;
                break;
            }
        }

        if (currFurn)
        {
            currFurn->handleEvent(event);
            
            switch (currFurn->getCurrentState())
            {
            case State::MOUSE_OVER_MOTION:
                cursor.isHovering = true;
                break;
            case State::MOUSE_OUT:
                cursor.isHovering = false;
                break;
            case State::MOUSE_DOWN:
                currFurn->isDragging = true;
                break;
            }

            bool canRotate = currFurn->isDragging || currFurn->getCurrentState() != State::MOUSE_OUT;
            if (event->type == SDL_MOUSEWHEEL && canRotate)
            {
                float rotationAmount = event->wheel.y * 5;
                currFurn->increaseRotation(rotationAmount);
            }
        }

        if (clickedBox)
        {
            for (auto i = boxes.begin(); i != boxes.end(); i++)
            {
                Box *box = *i;
                if (box == clickedBox)
                {
                    boxes.erase(i);
                    currFurn = box->furniture;
                    currFurn->setPosition(box->getPosition()); // TODO: need to make centered?
                    free(box);
                    break;
                }
            }
        }

        harold->handleEvent(event);

        switch (event->type)
        {
        case SDL_MOUSEBUTTONDOWN:
            cursor.isClosed = true;
            break;
        case SDL_MOUSEBUTTONUP:
            cursor.isClosed = false;
            if (currFurn)
                currFurn->isDragging = false;
            break;
        case SDL_KEYDOWN:
            if (event->key.keysym.sym == SDLK_e)
                placeFurn();
            break;
        }
    }

    void Game::update(float avgFPS)
    {
        std::stringstream fpsTextStream;
        fpsTextStream.str("");
        fpsTextStream << "Average FPS: " << avgFPS;

        fpsText.updateContent(fpsTextStream.str());
        fpsText.loadTexture(renderer);

        if (currFurn)
        {
            cursor.isHovering = currFurn->getCurrentState() != State::MOUSE_OUT;
        }
        else
        {
            cursor.isHovering = false;
            for (auto box : boxes)
            {
                if (box->getCurrentState() != State::MOUSE_OUT)
                {
                    cursor.isHovering = true;
                    break;
                }
            }
        }

        Vector2D mousePos = getMousePos();
        if (currFurn && currFurn->isDragging)
        {
            Vector2D moveDir = mousePos - currFurn->getPosition();

            float deltaTime = 1 / avgFPS; // TODO: pass in as parameter instead?
            currFurn->setVelocity(moveDir * deltaTime * 10); // TODO: make heavier objects more sluggish

            // TODO: make unable to move out of bounds
        }

        if (currFurn)
        {
            currFurn->move();
            currFurn->rotate();
            std::vector<GameObject *> others;
            for (auto furn : placedFurn)
                others.push_back(furn);
            currFurn->handleCollisions(others);
        }
        for (auto furn : placedFurn)
        {
            furn->move();
            furn->rotate();
            std::vector<GameObject *> others;
            for (auto otherFurn : placedFurn)
                if (furn != otherFurn)
                    others.push_back(furn);
            if (currFurn)
                others.push_back(currFurn);
            furn->handleCollisions(others);
        }

        if (currFurn && currFurn->isDragging)
        {
            Vector2D newPos = currFurn->getPosition() + currFurn->getSize() / 2 - cursor.getSize() / 2;
            cursor.setPosition(newPos);
        }
        else
            cursor.setPosition(mousePos - cursor.getSize() / 2);

        harold->move();
    }
}
