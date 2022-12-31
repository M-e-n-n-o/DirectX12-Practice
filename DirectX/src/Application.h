#pragma once

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// Own Headers
#include "Event.h"
#include "Window.h"
#include "CommandQueue.h"
#include "SwapChain.h"

#define USE_WARP_ADAPTER 0
#define SWAPCHAIN_BUFFER_COUNT 3

class Application : public EventListener
{
public:
	Application(const WindowSettings& windowSettings);
	virtual ~Application();

	void run();

	virtual void onEvent(Event& event) override;

	std::shared_ptr<Window> getWindow() const { return m_window; }

	static Application* Get() { return s_instance; }

private:
	void update();
	void render();

	virtual void onUpdate() = 0;
	virtual void onRender() = 0;

	Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(bool useWarp);
	Microsoft::WRL::ComPtr<ID3D12Device2> createDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);

protected:
	bool									m_isRunning = false;

	Microsoft::WRL::ComPtr<ID3D12Device2>	m_device;

	std::shared_ptr<Window>					m_window;
	std::shared_ptr<CommandQueue>			m_commandQueue;

private:
	static Application*						s_instance;
};