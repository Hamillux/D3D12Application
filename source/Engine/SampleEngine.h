#pragma once

#include "Engine.h"

class SampleEngine : public Engine
{
protected:
	virtual void Update(float deltaTime) override;
	virtual void Render(RenderContext& context) override;

private:
	float _time = 0.f;
};
