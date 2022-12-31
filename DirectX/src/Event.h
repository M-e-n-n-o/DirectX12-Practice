#pragma once

enum class EventType
{
	None = 0,

	WindowUpdate,
	WindowSizeChange,
	WindowDestroy,

	SysKeyDown,
	SysChar,
	KeyDown
};

struct Event
{
	EventType type;

	Event(const EventType& type) : type(type) {}
	virtual ~Event() = default;
};

struct KeyEvent : public Event
{
	char key;

	KeyEvent(const EventType& type, char key) : Event(type), key(key) {}
};

struct ResizeEvent : public Event
{
	uint32_t width;
	uint32_t height;

	ResizeEvent(uint32_t width, uint32_t height): Event(EventType::WindowSizeChange), width(width), height(height) {}
};

class EventListener
{
public:
	virtual ~EventListener() = default;

	virtual void onEvent(Event& event) = 0;
};