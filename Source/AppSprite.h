#pragma once

#include <Core/AppFramework.h>
#include <Core/ViewerBase.h>

struct FRecentFiles
{
	std::string VisibleName;
};

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

	bool ShowModal_WindowQuit();
	bool ShowModal_WindowNewCanvas();

	void Quit() { bOpen = false; }
	void Callback_OpenFile(std::filesystem::path FilePath);

	bool bOpen;

	// popup menu 'New Canvas'
	bool bRectangularCanvas;
	bool bRoundingToMultipleEight;
	int32_t CanvasCounter;
	ImVec2 OriginalCanvasSize;
	ImVec2 NewCanvasSize;
	ImVec2 Log2CanvasSize;
	char NewCanvasNameBuffer[BUFFER_SIZE_INPUT] = "";
	char NewCanvasWidthBuffer[BUFFER_SIZE_INPUT] = "";
	char NewCanvasHeightBuffer[BUFFER_SIZE_INPUT] = "";

	std::shared_ptr<SViewerBase> Viewer;
	std::vector<FRecentFiles> RecentFiles;
	std::map<std::string, std::string> ScriptFiles;
};
