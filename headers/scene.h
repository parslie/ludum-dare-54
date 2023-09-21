#ifndef CANVAS_H
#define CANVAS_H

#include <SDL2/SDL.h>
#include <vector>
#include "./game_object.h"

class Canvas
{
	public:
		Canvas();

        void render(SDL_Renderer* renderer);

        void addObj(GameObject* obj);

    private:
        std::vector<GameObject*> objs;
};

#endif // CANVAS_H