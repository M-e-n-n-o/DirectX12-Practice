#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>

// STL Headers
#include <algorithm>

// Own Headers
#include "Event.h"

struct WindowSettings
{
	const wchar_t* title;
	uint32_t width;
	uint32_t height;
	HINSTANCE hInstance;
};

class Window
{
public:
	Window(const WindowSettings& settings, EventListener* listener);
	~Window();

	void createSwapChain(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount);

	void show();

	void resize();

	bool isVSync() { return m_vSync; }
	void setVSync(bool vSync) { m_vSync = vSync; }

	bool isFullscreen() const { return m_fullscreen; }
	void setFullscreen(bool fullscreen);

	HWND getWindowHandle() const { return m_windowHandle; }

	uint32_t getBackBufferIndex() const;

private:
	void registerWindowClass(HINSTANCE hInstance, const wchar_t* windowClassName);
	void createWindow(const wchar_t* windowClassName, HINSTANCE hInstance, const wchar_t* appName, uint32_t width, uint32_t height);

private:
	HWND m_windowHandle = nullptr;
	RECT m_windowRect;

	bool m_vSync = true;

	bool m_tearingSupported = false;
	bool m_fullscreen = false;

	uint32_t m_width;
	uint32_t m_height;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
};