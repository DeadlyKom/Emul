#pragma once

#include <Core/AppFramework.h>
#include "UI/Viewer.h"

class FAppDebugger : public FAppFramework
{
public:
	FAppDebugger();

	// virtual functions
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;
	virtual bool IsOver() override;

private:
	void OnINT();
	std::shared_ptr<SViewer> Viewer;

	std::list<float> V;
};
