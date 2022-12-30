#pragma once

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// STL Headers
#include <memory>

// Own Headers
#include "Event.h"
#include "Window.h"
#include "CommandQueue.h"

#define BACK_BUFFER_COUNT 3

struct ApplicationSettings
{
	bool useWarpAdapter;
};

class Application : public EventListener
{
public:
	Application(const ApplicationSettings& appSettings, const WindowSettings& windowSettings);
	~Application();

	void start();
	
	void onEvent(Event& event) override;

private:
	void update();
	void render();

	Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(bool useWarp);
	void createDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);

private:
	std::unique_ptr<Window> m_window;

	Microsoft::WRL::ComPtr<ID3D12Device2> m_device;

	std::shared_ptr<CommandQueue> m_commandQueue;

	bool m_hasStarted = false;
};