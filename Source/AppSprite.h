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
	virtual std::vector<std::wstring> DragAndDropExtensions() const override { return { L".png" }; }
	virtual void DragAndDropFile(const std::filesystem::path& FilePath) override;
	virtual void LoadSettings() override;

private:
	void Show_MenuBar();

	std::shared_ptr<SViewerBase> Viewer;
	std::map<std::string, std::string> ScriptFiles;
};
