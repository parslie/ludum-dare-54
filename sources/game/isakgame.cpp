#include "../../headers/game/game.h"

#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>
#include <fstream>

#include "../../headers/game_object.h"
#include "../../headers/scene.h"
#include "../../headers/utils/constants.h"

#include "../../headers/game/house_generator.h"
#include "../../headers/game/room.h"

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
        , fpsText({"", 0, 96})
    {
        audioSource.addMusic( "./resources/main_menu.wav" );
        audioSource.addMusic( "./resources/Gamejam.wav" );
        audioSource.addMusic( "./resources/Harold_the_Hoarder_theme.wav" );
        audioSource.addSound( "./resources/wrong_placement.wav" );
        audioSource.addSound( "./resources/correct_placement.wav"    );

        HouseGenerator houseGenerator{};
        rooms = houseGenerator.generateRooms();
        walls = houseGenerator.generateWalls();

        FurnitureLoader loader{};
        loader.loadFurnitureData("./resources/furniture/furniture_meta_data.txt");
        boxes = loader.loadBoxes(renderer, houseGenerator.dir);
        furnitureAmount = boxes.size();

        scoreText       = new Text{ "Score:0",                                                              0,                48                 };
        tutorialText    = new Text{ "Press [E] to place furniture. Use [Mouse Wheel] to rotate furniture.", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 28 };
        placedFurnText  = new Text{ "Placed:0/" + std::to_string(furnitureAmount),                          0,                0                  };
        currentFurnText = new Text{ "Placeholder",                                                          0,                0                  };

        tutorialText->getPosition().set(SCREEN_WIDTH / 2 - (tutorialText->getContent().length()*28/3)/2, SCREEN_HEIGHT - 28 - 14);

        harold = new Harold({SCREEN_WIDTH / 2, 64});
    }

    void Game::reset()
    {
        placedFurn.clear();

        currFurn = nullptr;
        checkpoint = nullptr;

        score = 0;
        gameStarted = false;
        gameOver = false;
        furnished = false;

        scoreText       = new Text{ "Score:0",                                                              0,                48                 };
        tutorialText    = new Text{ "Press [E] to place furniture. Use [Mouse Wheel] to rotate furniture.", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 28 };
        placedFurnText  = new Text{ "Placed:0/" + std::to_string(furnitureAmount),                          0,                0                  };
        currentFurnText = new Text{ "Placeholder",                                                          0,                0                  };

        tutorialText->getPosition().set(SCREEN_WIDTH / 2 - (tutorialText->getContent().length()*28/3)/2, SCREEN_HEIGHT - 28 - 14);

        HouseGenerator houseGenerator{};
        rooms = houseGenerator.generateRooms();
        walls = houseGenerator.generateWalls();

        FurnitureLoader loader{};
        loader.loadFurnitureData("./resources/furniture/furniture_meta_data.txt");
        boxes = loader.loadBoxes(renderer, houseGenerator.dir);
        furnitureAmount = boxes.size();

        harold = new Harold({SCREEN_WIDTH / 2, 64});

        loadMedia();
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
        
        //TODO: free other unfreed objects
    }

    bool Game::loadMedia()
    {
        bool success = true;

        background.getTexture().loadFromFile(         "./resources/grass.png",      renderer);
        mainMenuBackground.getTexture().loadFromFile( "./resources/harold_start_screen.png",   renderer);
        highscoreBackground.getTexture().loadFromFile("./resources/end_screen.png", renderer);

        for (auto* room : rooms)
        {
            room->loadFloorImage(renderer);
            room->loadNameText(renderer);
        }

        for (auto* wall : walls)
        {
            wall->loadTexture(renderer);
        }

        fpsText.loadFont(         "./resources/fonts/bebasneue-regular.ttf", 48);
        scoreText->loadFont(      "./resources/fonts/bebasneue-regular.ttf", 48);
        tutorialText->loadFont(   "./resources/fonts/bebasneue-regular.ttf", 28);
        placedFurnText->loadFont( "./resources/fonts/bebasneue-regular.ttf", 48);
        currentFurnText->loadFont("./resources/fonts/bebasneue-regular.ttf", 14);

        fpsText.loadTexture(         renderer);
        scoreText->loadTexture(      renderer);
        tutorialText->loadTexture(   renderer);
        placedFurnText->loadTexture( renderer);
        currentFurnText->loadTexture(renderer);
        
        cursor.loadTexture(renderer);

        for (auto box : boxes)
        {
            box->loadTexture(renderer);
            box->furniture->loadTexture(renderer);
        }

		Mix_PlayMusic( audioSource.getMusic(0), -1 );
        
        harold->loadAnimation(renderer);

        return success;
    }

    void Game::render()
    {
        if (!gameStarted)
        {
            mainMenuBackground.render(renderer);
        }
        else
        {
            background.render(renderer);

            for (auto* room : rooms)
            {
                room->render(renderer);
            }

            for (auto* wall : walls)
            {
                wall->render(renderer);
            }

            for (auto box : boxes)
                box->render(renderer);
            
            if (currFurn != nullptr) 
            {
                currFurn->render(renderer);
                currentFurnText->render(renderer);
            }
            
            for (auto furniture : placedFurn)
                furniture->render(renderer);

            if(checkpoint != nullptr)
            {
                checkpoint->render(renderer);
            }

            harold->renderAnimation(renderer);

            //fpsText.render(renderer);
            scoreText->render(renderer);
            placedFurnText->render(renderer);
            tutorialText->render(renderer);
        }

        if(gameOver)
        {
            highscoreBackground.render(renderer);

            for(auto* highscore : highscores)
            {
                highscore->render(renderer);
            }
        }

        cursor.updateTexture(renderer);
        cursor.render(renderer);
    }

    void Game::placeFurn()
    {
        if (currFurn)
        {
            placedFurn.push_back(currFurn);

            int x = currFurn->getPosition().getX();
            int y = currFurn->getPosition().getY();

            Room* activeRoom = nullptr;
            for (auto* room : rooms)
            {
                if(room->isInside(x, y))
                {
                    activeRoom = room;
                    break;
                }
            }

            if (activeRoom)
            {
                std::string roomName = activeRoom->getName();
                if(currFurn->compatableWith(roomName))
                {
                    score += 10;
                    activeRoom->setColor(0, 255, 0);
                    Mix_PlayChannel( -1, audioSource.getSound(1), 0 );
                }
                else
                {
                    score -= 10;
                    activeRoom->setColor(255, 0, 0);
                    Mix_PlayChannel( -1, audioSource.getSound(0), 0 );
                }
            }
            else
            {
                score -= 10;
                // TODO: flash background red?
                Mix_PlayChannel( -1, audioSource.getSound(0), 0 );
            }

            scoreText->updateContent("Score:" + std::to_string(score));
            scoreText->loadTexture(renderer);

            placedFurnText->updateContent("Placed:" + std::to_string(placedFurn.size()) + "/" + std::to_string(furnitureAmount));
            placedFurnText->loadTexture(renderer);

            if(boxes.size() == 0) // Done furnishing
            {
                furnished = true;

                // Pick furniture randomly from placedFurn.
                size_t amount = placedFurn.size()/2;
                std::sample(
                    placedFurn.begin(),
                    placedFurn.end(),
                    std::back_inserter(furnToVisit),
                    amount,
                    std::mt19937{std::random_device{}()}
                );

                currFurnToVisit = furnToVisit[0];
                checkpoint = new GameObject{{currFurnToVisit->getPosition().getX()-32+currFurnToVisit->getSize().getX()/2, currFurnToVisit->getPosition().getY()-32+currFurnToVisit->getSize().getY()/2}, {64, 64}, {0, 0}, "checkpoint", "./resources/ring.png"};
                checkpoint->loadTexture(renderer);

                harold->canControl = true;
            }

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
                currFurn->setRotationDirection(RotDir::NONE);
                currFurn->setRotationSpeed(0.0f);
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

                    Vector2D furnPos = box->getPosition() + box->getSize() / 2 - currFurn->getSize() / 2;
                    currFurn->setPosition(furnPos);

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
            if (event->key.keysym.sym == SDLK_r)
            {
                if(gameOver)
                {
                    reset();
                }
            }
            if (event->key.keysym.sym == SDLK_p)
            {
                gameStarted = true;
		        Mix_PlayMusic( audioSource.getMusic(1), -1 );
            }
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
            currentFurnText->setPosition(currFurn->getPosition().getX(), currFurn->getPosition().getY()-currFurn->getSize().getY());
            currentFurnText->updateContent(currFurn->getName());
            currentFurnText->loadTexture(renderer);
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

            float pullSpeed = 20 / currFurn->getMass(); // TODO: finjustera
            currFurn->setVelocity(moveDir * deltaTime * pullSpeed);
        }

        if (currFurn)
        {
            currFurn->move();
            currFurn->rotate();
            std::vector<GameObject *> others;
            for (auto furn : placedFurn)
                others.push_back(furn);
            currFurn->handleCollisions(others);

            bool hadCollision = false;
            for (auto* other: walls)
            {
                if(currFurn->hasCollision(other->getCorners()))
                {
                    hadCollision = true;
                }
            }
            currFurn->handleCollisions(walls);

            if(hadCollision)
            {
                placeFurn();
            }
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
            furn->handleCollisions(walls);
        }

        if (currFurn && currFurn->isDragging)
        {
            Vector2D newPos = currFurn->getPosition() + currFurn->getSize() / 2 - cursor.getSize() / 2;
            cursor.setPosition(newPos);
        }
        else
            cursor.setPosition(mousePos - cursor.getSize() / 2);

        harold->move();

        if (!gameOver && furnished)
        {
            if(furnToVisit.size() == 0) // "GAME OVER"
            {
                std::ifstream file("./resources/scoreboard.txt");
                std::vector<int> scoreboard;

                int num;
                while (file >> num) {
                    scoreboard.push_back(num);
                }
                file.close();

                scoreboard.push_back(score);

                std::sort(scoreboard.begin(), scoreboard.end(), std::greater<int>());

                std::ofstream out("./resources/scoreboard.txt");
                for (const auto& highscore : scoreboard) {
                    out << highscore << std::endl;
                }
                out.close();

                // Load highscores into vector
                std::ifstream f("./resources/scoreboard.txt");

                std::vector<std::string> lines;
                std::string line;

                int count = 0;
                while (std::getline(f, line) && count < 10) {
                    lines.push_back(line);
                    count++;
                }

                highscores.clear();
                int offset = 0;
                for (const auto& l : lines) {
                    offset++;
                    Text* t = new Text{std::to_string(offset) + ") " + l, SCREEN_WIDTH/2, 28*offset};
                    t->loadFont("./resources/fonts/bebasneue-regular.ttf", 28);
                    t->loadTexture(renderer);
                    highscores.push_back(t);
                }

                f.close(); 

                gameOver = true;
		        Mix_PlayMusic( audioSource.getMusic(2), -1 );
            }
            else // Harold completes task
            {
                for (auto* other: walls)
                {
                    if(harold->hasCollision(other))
                    {
                        harold->getPosition().set(SCREEN_WIDTH / 2, 64);
                        Mix_PlayChannel( -1, audioSource.getSound(0), 0 );
                    }
                }

                checkpoint->getPosition().set(currFurnToVisit->getPosition().getX()-32+currFurnToVisit->getSize().getX()/2, currFurnToVisit->getPosition().getY()-32+currFurnToVisit->getSize().getY()/2);
                if(checkpoint->isInside(harold->getPosition().getX(), harold->getPosition().getY()))
                {
                    Mix_PlayChannel( -1, audioSource.getSound(1), 0 );
                    furnToVisit.erase(furnToVisit.begin());
                    if(furnToVisit.size() > 0)
                    {
                        currFurnToVisit = furnToVisit[0];
                    }
                }
            }
        }
    }
}
