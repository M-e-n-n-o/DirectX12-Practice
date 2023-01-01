#pragma once

#include "Window.h"
#include "Event.h"

class Game
{
public:
	Game() = default;
	virtual ~Game() = default;

	virtual std::shared_ptr<Window> Initialize(const WindowSettings& settings) = 0;
	virtual void Destory() = 0;

	virtual void onUpdate() = 0;
	virtual void onRender() = 0;

	virtual void onKeyPressed(KeyEvent& event) = 0;
	virtual void onResize(ResizeEvent& event) = 0;
};