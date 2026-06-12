#pragma once

#include <Core/AppFramework.h>
#include <Core/ViewerBase.h>

class SCanvas;

enum class EImageFormat
{
	None,
	Create,
	JSON,
	PNG,
	GIF,
	Aseprite,
};

enum class EFrameMode
{
	None,
	Difference
};

struct FViewFlags
{
	bool bGrid = false;
	bool bPixelGrid = true;
	bool bAttributeGrid = false;
	bool bAlphaCheckerboardGrid = true;
	bool bTransparentMask = false;
	EFrameMode FrameMode = EFrameMode::None;
	ImVec2 GridSettingSize = ImVec2(8.0f, 8.0f);
	ImVec2 GridSettingOffset = ImVec2(0.0f, 0.0f);
	ImVec4 TransparentColor = ImVec4(0.169f, 0.396f, 0.925f, 0.0f);
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
	virtual std::vector<std::wstring> DragAndDropExtensions() const override { return { L".json", L".png", L".aseprite" }; }
	virtual void DragAndDropFile(const std::filesystem::path& FilePath) override;
	virtual void LoadSettings() override;

	static EImageFormat SupportImageFormat(const std::filesystem::path& FilePath);
	static bool Import_Image(const std::shared_ptr<SViewerBase>& Viewer, const std::filesystem::path& FilePath, EImageFormat ImageFormat);

private:
	void SetupHotKeys();

	void Show_MenuBar();

	bool ShowModal_WindowQuit();
	bool ShowModal_WindowNewCanvas();
	bool ShowModal_WindowgGridSettings();
	bool ShowModal_WindowgCodeGeneration();

	void Input_HotKeys();
	void Imput_Close();
	void Imput_CloseAll();

	void Quit() { bOpen = false; }
	bool OpenFile(const std::filesystem::path& FilePath);
	void Import_JSON(const std::filesystem::path& FilePath);
	bool Callback_OpenFile(const std::filesystem::path& FilePath);
	bool HasCanvasWithTimeline() const;

	std::shared_ptr<SCanvas> GetActiveCanvas();

	bool bOpen;
	FViewFlags ViewFlags;

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

	// popup menu 'Grid Settings' 
	char GridSettingsWidthBuffer[BUFFER_SIZE_INPUT] = "";
	char GridSettingsHeightBuffer[BUFFER_SIZE_INPUT] = "";
	char GridSettingsOffsetXBuffer[BUFFER_SIZE_INPUT] = "";
	char GridSettingsOffsetYBuffer[BUFFER_SIZE_INPUT] = "";
	bool bTmpGrid;
	ImVec2 TmpGridSettingSize;
	ImVec2 TmpGridSettingOffset;

	//popup menu 'Export'
	int32_t ExportCounter;
	char NewOutputFileNameBuffer[BUFFER_SIZE_INPUT] = "";

	std::shared_ptr<SViewerBase> Viewer;
	std::vector<std::filesystem::path> RecentFiles;
	std::map<std::string, std::string> ScriptFiles;

	std::vector<FHotKey> Hotkeys;
	std::vector<std::shared_ptr<SCanvas>> ActiveCanvas;
};
