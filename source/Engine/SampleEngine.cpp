#include "SampleEngine.h"
#include <algorithm>
#include <cmath>

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

	context.command->ClearRenderTargetView(context.renderTargetView, color, 0, nullptr);
}
