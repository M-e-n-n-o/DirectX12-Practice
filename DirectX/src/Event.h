#pragma once

enum class EventType
{
	None = 0,

	Paint,
	SysKeyDown,
	KeyDown,
	SysChar,
	Size,
	Destroy
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

class EventListener
{
public:
	virtual ~EventListener() = default;

	virtual void onEvent(Event& event) = 0;
};