#pragma once

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// Own Headers
#include "Event.h"
#include "Window.h"
#include "Game.h"

#define USE_WARP_ADAPTER 0

class Application : public EventListener
{
public:
	Application(const WindowSettings& windowSettings, Game* game);
	virtual ~Application();

	void run();

	virtual void onEvent(Event& event) override;

	std::shared_ptr<Window> getWindow() const { return m_window; }

	Microsoft::WRL::ComPtr<ID3D12Device2> getDevice() const { return m_device; }

	uint64_t getFrameCount() const { return m_frameCount; }

	static Application* Get() { return s_instance; }

private:
	void update();
	void render();

	Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(bool useWarp);
	Microsoft::WRL::ComPtr<ID3D12Device2> createDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);

protected:
	bool									m_isRunning = false;

	Game*									m_game;

	Microsoft::WRL::ComPtr<ID3D12Device2>	m_device;

	std::shared_ptr<Window>					m_window;

	uint64_t								m_frameCount;

private:
	static Application*						s_instance;
};