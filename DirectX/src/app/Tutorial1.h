#pragma once

#include "dxpch.h"

#include <chrono>
#include "Application.h"
#include "CommandQueue.h"
#include "SwapChain.h"

#define SWAPCHAIN_BUFFER_COUNT 3

class Tutorial1 : public Game
{
private:
    uint64_t frameFenceValues[SWAPCHAIN_BUFFER_COUNT] = {};

    std::shared_ptr<Window> window;
    std::shared_ptr<SwapChain> swapChain;
    std::shared_ptr<CommandQueue> commandQueue;

public:
    Tutorial1() = default;
    ~Tutorial1() = default;

    std::shared_ptr<Window> Initialize(const WindowSettings& settings) override
    {
        auto device = Application::Get()->getDevice();

        window = std::make_shared<Window>(settings, Application::Get());

        commandQueue = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

        swapChain = std::make_shared<SwapChain>(device, commandQueue->getCommandQueue(), settings.width, settings.height,
            SWAPCHAIN_BUFFER_COUNT, window->getWindowHandle(), settings.tearingSupported);
        window->setSwapChain(swapChain);

        return window;
    }

    void Destory() override
    {
        commandQueue->flush();
    }

	void onUpdate() override
	{
        static uint64_t frameCounter = 0;
        static double elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto t0 = clock.now();

        frameCounter++;
        auto t1 = clock.now();
        auto deltaTime = t1 - t0;
        t0 = t1;
        elapsedSeconds += deltaTime.count() * 1e-9;
        if (elapsedSeconds > 1.0)
        {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            wchar_t wbuffer[500];
            size_t converted;
            ::mbstowcs_s(&converted, wbuffer, buffer, strlen(buffer) + 1);
            LPWSTR str = wbuffer;
            OutputDebugString(str);

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }
	}

	void onRender() override
	{
        auto backBuffer = swapChain->getCurrentBackBuffer();
        auto commandList = commandQueue->getCommandList();

        // Clear the render target.
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                backBuffer.Get(),
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrier);

            FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(swapChain->getCurrentRenderTargetView());

            commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        }

        // Present
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                backBuffer.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            commandList->ResourceBarrier(1, &barrier);

            commandQueue->executeCommandList(commandList);

            swapChain->present();

            // Wait until the new backbuffer is ready to be used
            frameFenceValues[swapChain->getCurrentBackBufferIndex()] = commandQueue->signal();
            commandQueue->waitForFenceValue(frameFenceValues[swapChain->getCurrentBackBufferIndex()]);
        }
	}

    void onKeyPressed(KeyEvent& event) override
    {

    }

    void onResize(ResizeEvent& event) override
    {
        commandQueue->flush();

        uint32_t currentBackBufferIndex = swapChain->getCurrentBackBufferIndex();
        for (int i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
        {
            frameFenceValues[i] = frameFenceValues[currentBackBufferIndex];
        }
    }
};