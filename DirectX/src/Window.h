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
#include "SwapChain.h"

struct WindowSettings
{
	const wchar_t* title;
	uint32_t width;
	uint32_t height;
	bool tearingSupported;
	HINSTANCE hInstance;
};

class Window
{
public:
	Window(const WindowSettings& settings, EventListener* listener);
	~Window() = default;

	void show();

	void onResize(uint32_t width, uint32_t height);

	uint32_t getWidth() { return m_width; }
	uint32_t getHeight() { return m_height; }

	bool isFullscreen() const { return m_fullscreen; }
	void setFullscreen(bool fullscreen);

	void setSwapChain(const std::shared_ptr<SwapChain>& swapChain) { m_swapChain = swapChain; }
	const std::shared_ptr<SwapChain> getSwapChain() const { return m_swapChain; }

	HWND getWindowHandle() const { return m_windowHandle; }

private:
	friend LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

	void registerWindowClass(HINSTANCE hInstance, const wchar_t* windowClassName);
	void createWindow(const wchar_t* windowClassName, HINSTANCE hInstance, const wchar_t* appName, uint32_t width, uint32_t height);

private:
	std::shared_ptr<SwapChain>		m_swapChain;

	HWND							m_windowHandle = nullptr;
	RECT							m_windowRect;

	bool							m_tearingSupported = false;
	bool							m_fullscreen = false;

	uint32_t						m_width;
	uint32_t						m_height;

	EventListener*					m_listener;
};