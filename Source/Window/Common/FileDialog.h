#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

enum class EFileSortOrder
{
	Unknown,
	Up,
	Down,
};

enum class EDialogMode
{
	Select,
	Open,
	Save
};

enum class EDialogStage
{
	Unknown,
	Close,
	Selected,
};

class SFileDialog : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SFileDialog;

public:
	static std::shared_ptr<SFileDialog> Get();
	static void OpenWindow(
		std::shared_ptr<SViewerBase> Viewer,
		const std::string& FileDialogName,
		EDialogMode Mode, 
		std::function<void(std::filesystem::path, EDialogStage Selected)>&& OnCallback,
		const std::filesystem::path& FilePath,
		const std::string& FilterTypes);
	static void CloseWindow();

	SFileDialog(EFont::Type _FontName, const std::wstring& Name = L"");
	virtual ~SFileDialog() = default;

	virtual void Initialize(const std::any& Arg) override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Release();
	void SetFilterTypes(const std::string& InFilterTypes);
	void ReadDirectory(const std::string& Path);
	void ShowDirectory(const ImVec2& Size);
	void ShowFiles(const ImVec2& Size);
	void ShowFilterDropBox();

	void ApplyFilterTypes();

	EFileSortOrder FlipFlopOrder(EFileSortOrder CurrentOrder)
	{
		NameSortOrder = SizeSortOrder = TypeSortOrder = DateSortOrder = EFileSortOrder::Unknown;
		return CurrentOrder == EFileSortOrder::Down ? EFileSortOrder::Up : EFileSortOrder::Down;
	}

	bool bAllFiles;

	EDialogMode DialogMode;
	EDialogStage DialogStage;

	EFileSortOrder NameSortOrder;
	EFileSortOrder SizeSortOrder;
	EFileSortOrder TypeSortOrder;
	EFileSortOrder DateSortOrder;

	int32_t FileSelectIndex;
	int32_t FolderSelectIndex;
	int32_t SelectedFilterIndex;

	std::string FileDialogName;
	std::string CurrentFile;
	std::string CurrentFolder;
	std::string CurrentPath;

	std::function<void(std::filesystem::path, EDialogStage Selected)> CloseCallback;
	std::vector<std::string> FilterTypes;
	std::vector<std::filesystem::directory_entry> Files;
	std::vector<std::filesystem::directory_entry> Folders;
};
