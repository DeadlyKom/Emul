#pragma once

#include <atomic>
#include <Core/AppFramework.h>
#include <Core/ViewerBase.h>
#include <Utils/6912/CodeGenerator.h>
#include <mutex>
#include <thread>

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

namespace EFrameMode
{
	enum Type
	{
		None,
		Difference,
		ReverseDifference,

		MAX
	};

	inline Type& operator++(Type& Value)
	{
		int32_t Next = (int)Value + 1;
		if (Next >= (int32_t)MAX)
		{
			Next = 0;
		}

		Value = (Type)Next;
		return Value;
	}

	inline Type operator++(Type& Value, int32_t)
	{
		Type OldValue = Value;
		++Value;
		return OldValue;
	}
}

struct FViewFlags
{
	bool bGrid;
	bool bPixelGrid;
	bool bAttributeGrid;
	bool bAlphaCheckerboardGrid;
	bool bTransparentMask;
	EFrameMode::Type FrameMode;
	ImVec2 GridSettingSize;
	ImVec2 GridSettingOffset;
	ImVec4 TransparentColor;

	FViewFlags()
		: bGrid(false)
		, bPixelGrid(true)
		, bAttributeGrid(false)
		, bAlphaCheckerboardGrid(true)
		, bTransparentMask(false)
		, FrameMode(EFrameMode::None)
		, GridSettingSize(8.0f, 8.0f)
		, GridSettingOffset(0.0f, 0.0f)
		, TransparentColor(0.169f, 0.396f, 0.925f, 0.0f)
	{}
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
	bool ShowModal_CodeGenerationProgress();

	bool Show_WindowgCodeGeneration();

	void Input_HotKeys();
	void Imput_Close();
	void Imput_CloseAll();
	void Imput_ToggleFrameMode();
	void Imput_ToggleFrameDirection();

	void Quit() { bOpen = false; }
	bool OpenFile(const std::filesystem::path& FilePath);
	void Import_JSON(const std::filesystem::path& FilePath);
	bool Callback_OpenFile(const std::filesystem::path& FilePath);
	bool HasCanvasWithTimeline() const;
	void RefreshCodeGenerationPreview();
	void StartCodeGenerationPreview();
	void PollCodeGenerationPreviewJob();
	bool ExportCodeGenerationPreview();

	std::shared_ptr<SCanvas> GetActiveCanvas();

	bool bOpen;
	FViewFlags ViewFlags;
	EFrameMode::Type FrameDifferenceDirection;

	// popup menu 'New Canvas'
	bool bRectangularCanvas;
	bool bRoundingToMultipleEight;
	int32_t CanvasCounter;
	char NewCanvasNameBuffer[BUFFER_SIZE_INPUT];
	char NewCanvasWidthBuffer[BUFFER_SIZE_INPUT];
	char NewCanvasHeightBuffer[BUFFER_SIZE_INPUT];
	ImVec2 OriginalCanvasSize;
	ImVec2 NewCanvasSize;
	ImVec2 Log2CanvasSize;

	// popup menu 'Grid Settings' 
	bool bTmpGrid;
	char GridSettingsWidthBuffer[BUFFER_SIZE_INPUT];
	char GridSettingsHeightBuffer[BUFFER_SIZE_INPUT];
	char GridSettingsOffsetXBuffer[BUFFER_SIZE_INPUT];
	char GridSettingsOffsetYBuffer[BUFFER_SIZE_INPUT];
	ImVec2 TmpGridSettingSize;
	ImVec2 TmpGridSettingOffset;

	//popup menu 'Export'
	bool bCodeGenerationGenerateOpcode;
	bool bCodeGenerationOpen;
	bool bCodeGenerationShowCode;
	bool bCodeGenerationApplyWindowSize;
	bool bCodeGenerationPreviewValid;
	bool bCodeGenerationCodeWindowSizeInitialized;
	std::atomic<bool> bCodeGenerationGenerationInProgress;
	bool bCodeGenerationProgressModalOpen;
	bool bCodeGenerationProgressShouldClose;
	bool bCodeGenerationLogScrollToBottom;
	std::atomic<bool> bCodeGenerationCancelRequested;
	std::atomic<int32_t> CodeGenerationProgressCurrent;
	std::atomic<int32_t> CodeGenerationProgressTotal;
	int32_t ExportCounter;
	float CodeGenerationWindowHeight;
	char NewOutputFileNameBuffer[BUFFER_SIZE_INPUT];
	char CodeGenerationLabelNameBuffer[BUFFER_SIZE_INPUT];
	ImVec2 CodeGenerationBaseWindowSize;
	ImVec2 CodeGenerationCodeWindowSize;
	CodeGenerator::FOptions CodeGenerationOptions;
	std::string CodeGenerationPreviewText;
	std::string CodeGenerationLogText;
	std::vector<uint8_t> CodeGenerationOpcodeBytes;
	std::thread CodeGenerationWorker;
	CodeGenerator::FResult CodeGenerationJobResult;

	std::shared_ptr<SViewerBase> Viewer;
	std::vector<std::filesystem::path> RecentFiles;
	std::map<std::string, std::string> ScriptFiles;

	std::vector<FHotKey> Hotkeys;
	std::vector<std::shared_ptr<SCanvas>> ActiveCanvas;
};
