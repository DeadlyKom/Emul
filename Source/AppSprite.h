#pragma once

#include <Core/AppFramework.h>
#include <Core/ViewerBase.h>

class FAppSprite : public FAppFramework
{
	using ThisClass = FAppSprite;

public:
	FAppSprite();

	// virtual functions
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;
	virtual bool IsOver() override;

private:
	void LoadIniSettings();
	void Show_MenuBar();

	std::shared_ptr<SViewerBase> Viewer;
};
