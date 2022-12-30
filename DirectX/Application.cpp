#include "Application.h"

// Helper functions
#include "Helpers.h"

using namespace Microsoft::WRL;

Application::Application(const ApplicationSettings& appSettings, const WindowSettings& windowSettings)
{
	m_window = std::make_unique<Window>(windowSettings, this);

	ComPtr<IDXGIAdapter4> adapter = getAdapter(appSettings.useWarpAdapter);
	createDevice(adapter);

	m_commandQueue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, BACK_BUFFER_COUNT, m_window->getBackBufferIndex());

	// Create swap chain (inside create descriptor heaps and RTV's)
	m_window->createSwapChain(m_commandQueue->getCommandQueue(), windowSettings.width, windowSettings.height, BACK_BUFFER_COUNT);

	// Create fence
}

Application::~Application()
{

}

void Application::start()
{
    m_hasStarted = true;

	m_window->show();

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	// Make sure the command queue has finished all commands before closing.
	//Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

	//::CloseHandle(g_FenceEvent);
}

void Application::onEvent(Event& event)
{
	if (!m_hasStarted)
	{
		return;
	}

	switch (event.type)
	{
	case EventType::Paint:
		update();
		render();
		break;
	case EventType::SysKeyDown:
	case EventType::KeyDown:
	{
		KeyEvent* e = dynamic_cast<KeyEvent*>(&event);

		bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

		switch (e->key)
		{
		case 'V':
			m_window->setVSync(!m_window->isVSync());
			break;
		case VK_ESCAPE:
			::PostQuitMessage(0);
			break;
		case VK_RETURN:
			if (alt)
			{
		case VK_F11:
			m_window->setFullscreen(!m_window->isFullscreen());
			}
			break;
		}
	}
	break;
	case EventType::SysChar:
		break;
	case EventType::Size:
	{
		m_window->resize();
	}
	break;
	case EventType::Destroy:
		::PostQuitMessage(0);
		break;
	default:
		break;
	}
}

void Application::update()
{
}

void Application::render()
{
}

ComPtr<IDXGIAdapter4> Application::getAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

void Application::createDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter)
{
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(m_device.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif
}
