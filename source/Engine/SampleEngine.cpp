#include "SampleEngine.h"

void SampleEngine::Update(float deltaTime)
{
	Engine::Update(deltaTime);

	_time += deltaTime;
}

void SampleEngine::Render(RenderContext& context)
{
	Engine::Render(context);

	const float phase = _time / 3.f * 3.14;
	const FLOAT val = std::clamp(std::sinf(phase) * std::sinf(phase), 0.f, 1.f);
	const FLOAT color[4] = { val, val, val, 1.f };

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{}, dsvHandle{};
	GetCurrentBackBufferHandles(rtvHandle, dsvHandle);
	context.command->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
}
