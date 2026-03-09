#include "SpriteList.h"
#include <Core/AppFramework.h>
#include <Utils/UI/Draw.h>
#include <Settings/SpriteSettings.h>
#include <Window/Common/FileDialog.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include <json/json.hpp>
#include <Utils/IO.h>
#include <AppSprite.h>
#include <Window/Sprite/Events.h>
#include "Canvas.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "resource.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Sprite List";
	const char* ExportToName = "Export##ExportTo";
	static const char* PopupMenuName = TEXT("##PopupMenu");

	static constexpr ImVec2 VisibleSizeArray[] =
	{
		ImVec2(16.0f, 16.0f),
		ImVec2(24.0f, 24.0f),
		ImVec2(32.0f, 32.0f),
		ImVec2(48.0f, 48.0f),
		ImVec2(64.0f, 64.0f),
		ImVec2(96.0f, 96.0f),
		ImVec2(128.0f, 128.0f),
		ImVec2(256.0f, 256.0f),
		ImVec2(512.0f, 512.0f),
	};

}

int32_t FSprite::StaticUniqueID = 0;

SSpriteList::SSpriteList(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
	, bNeedKeptOpened_ExportPopup(false)
	, ScaleVisible(2)
	, bUniqueExportFilename(false)
	, IndexSelectedSprite(INDEX_NONE)
	, IndexRenameSprite(INDEX_NONE)
	, IndexSelectedScript(INDEX_NONE)
{}

void SSpriteList::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_Sprite>(
		[this](const FEvent_Sprite& Event)
		{
			if (Event.Tag == FEventTag::AddSpriteTag)
			{
				AddSprite(
					Event.SpriteRect,
					Event.SpriteName,
					Event.SourcePathFile,
					Event.Width,
					Event.Height,
					Event.IndexedData,
					Event.InkData,
					Event.AttributeData,
					Event.MaskData);
			}
			else if (Event.Tag == FEventTag::UpdateSpriteTag)
			{
				std::vector<std::shared_ptr<FSprite>> SelectedSprites = UpdateSprite(
					Event.SourcePathFile,
					Event.IndexedData,
					Event.InkData,
					Event.AttributeData,
					Event.MaskData
				);

				ExportSprites("", CurrentPath, SelectedSprites, false);
			}
		});
	SubscribeEvent<FEvent_ImportJSON>(
		[this](const FEvent_ImportJSON& Event)
		{
			std::vector<std::shared_ptr<FSprite>> ReadSprites;
			if (ImportSprites(Event.FilePath, ReadSprites))
			{
				CurrentPath = Event.FilePath.parent_path();;
				ApplyImportSprites(ReadSprites);
			}
		});
	SubscribeEvent<FEvent_Sprite>(
		[this](const FEvent_Sprite& Event)
		{
			if (Event.Tag == FEventTag::RequestAllSpritesTag)
			{
				for (std::shared_ptr<FSprite> Sprite : Sprites)
				{
					FEvent_Sprite Event;
					Event.Tag = FEventTag::ResponseAllSpritesTag;
					Event.SpriteName = Sprite->Name;
					Event.UniqueID = Sprite->UniqueID;
					SendEvent(Event);
				}
			}
		});
}

void SSpriteList::Initialize(const std::vector<std::any>& Args)
{
	CurrentPath = FAppFramework::GetPath(EPathType::Export);

	FSpriteSettings& SpriteSettings = FSpriteSettings::Get();
	auto ScriptFilesOptional = SpriteSettings.GetValue<std::map<std::string, std::string>>(
		{ FSpriteSettings::ScriptFilesTag, typeid(std::map<std::string, std::string>) });

	if (ScriptFilesOptional.has_value())
	{
		ScriptFiles = ScriptFilesOptional.value();
	}
}

void SSpriteList::SetupHotKeys()
{
	auto Self = std::dynamic_pointer_cast<SSpriteList>(shared_from_this());
	Hotkeys =
	{
		{ ImGuiMod_Ctrl | ImGuiKey_A,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SelectAll,						Self)	},	// (ctrl + A)

		{ ImGuiKey_Escape,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Escape,							Self)	},	// cansel
		{ ImGuiKey_Delete,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Delete,							Self)	},	// (delete)
	};
}

void SSpriteList::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);

	PopupMenu_EditMetadataID = ImGui::GetCurrentWindow()->GetID(PopupMenuName);
	{
		Input_HotKeys();
		Input_Mouse();

		float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("##Scrolling", ImVec2(0, -FooterHeight), false,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoDecoration))
		{
			SpriteListID = ImGui::GetCurrentWindow()->ID;
			Draw_SpriteList();
		}
		ImGui::EndChild();
		ImGui::Separator();

		// button export
		{
			bool bEnabled = Sprites.size() > 0;
			if (bEnabled)
			{
				for (const std::shared_ptr<FSprite>& Sprite : Sprites)
				{
					bEnabled = Sprite->bSelected;
					if (bEnabled)
					{
						break;
					}
				}
			}

			if (!bEnabled) ImGui::BeginDisabled();

			const ImGuiID ExportToID = ImGui::GetCurrentWindow()->GetID(ExportToName);
			if (bNeedKeptOpened_ExportPopup || ImGui::Button("Export", ImVec2(0.0f, 0.0f)))
			{
				bNeedKeptOpened_ExportPopup = false;
				ImGui::OpenPopup(ExportToID);
			}
			if (ImGui::BeginPopupModal(ExportToName, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
			{
				Draw_ExportSprites();
			}
			if (!bEnabled) ImGui::EndDisabled();
			ImGui::SameLine();
		}

		// draw current scale
		{
			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
			UI::TextAligned(std::format("x{}", VisibleSizeArray[ScaleVisible].x).c_str(), { 1.0f, 0.5f });
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}

	if (ImGui::BeginPopupEx(PopupMenu_EditMetadataID,
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoSavedSettings))
	{
		if (IndexSelectedSprite != INDEX_NONE && Sprites.size() > IndexSelectedSprite)
		{
			const bool bSelected = Sprites[IndexSelectedSprite]->bSelected;
			if (!bSelected && ImGui::MenuItem("Select"))
			{
				Sprites[IndexSelectedSprite]->bSelected = true;
			}
			if (bSelected && ImGui::MenuItem("Deselect"))
			{
				Sprites[IndexSelectedSprite]->bSelected = false;
			}
		}
		if (ImGui::MenuItem("Edit Meta"))
		{
			SendSelectedSprite();
		}
		ImGui::EndPopup();
	}
}

void SSpriteList::Destroy()
{
	UnsubscribeAll();
}

void SSpriteList::Input_HotKeys()
{
	Shortcut::Handler(Hotkeys);
}

void SSpriteList::Input_Mouse()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	const bool bHovered = ImGui::IsWindowHovered();
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;
	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != SpriteListID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		ScaleVisible = (uint32_t)ImClamp<float>(ScaleVisible + MouseWheel, 0.0f, IM_ARRAYSIZE(VisibleSizeArray) - 1);
	}

	if (!ImGui::IsWindowFocused())
	{
		return;
	}
}

void SSpriteList::Imput_SelectAll()
{
	for (const std::shared_ptr<FSprite>& Sprite : Sprites)
	{
		Sprite->bSelected = true;
	}
}

void SSpriteList::Imput_Escape()
{
	for (const std::shared_ptr<FSprite>& Sprite : Sprites)
	{
		Sprite->bSelected = false;
	}
}

void SSpriteList::Imput_Delete()
{
	if (IndexSelectedSprite < 0 && IndexSelectedSprite >= Sprites.size())
	{
		return;
	}
	Sprites.erase(Sprites.begin() + IndexSelectedSprite);
}

void SSpriteList::Draw_SpriteList()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(50.0f, 65.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));

	const ImGuiStyle& Style = ImGui::GetStyle();
	const ImVec2 VisibleSize = VisibleSizeArray[ScaleVisible];
	const float WindowVisible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	
	const uint32_t SpriteNum = uint32_t(Sprites.size());
	for (uint32_t Index = 0; Index < SpriteNum; ++Index)
	{
		ImGui::PushID(Index);
		const std::string EditingSpriteName = std::format("{}_SpriteName{}", Sprites[Index]->StaticUniqueID, Index);

		std::shared_ptr<FSprite>& Sprite = Sprites[Index];
		const std::string StringID = std::format("SpriteButton##%{}", Index);

		ImGui::BeginGroup();
		bool bHovered = false;
		if (UI::Draw_ButtonZXColorSprite(StringID.c_str(), Sprite, VisibleSize, {}, &bHovered))
		{
			IndexSelectedSprite = Index;
			SendSelectedSprite();

			{
				ImGuiContext& Context = *ImGui::GetCurrentContext();
				if (Context.IO.MouseReleased[ImGuiMouseButton_Left] && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
				{
					Sprite->bSelected = !Sprite->bSelected;
				}
			}
		}
		
		// sprite Name
		{
			ImVec2 PrevSize = ImGui::GetItemRectSize();
			ImGui::PushItemWidth(PrevSize.x);

			const bool bIsSelectedSprite = IndexSelectedSprite == Index;
			// Enable editing by pressing F2
			if (bIsSelectedSprite && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
			{
				EditingSprites[EditingSpriteName] = true;
			}

			// Enable double-click editing
			if (EditingSprites[EditingSpriteName])
			{
				ImGui::SetKeyboardFocusHere();

				char InputBuffer[128];
				std::strncpy(InputBuffer, Sprite->Name.c_str(), sizeof(InputBuffer));
				InputBuffer[sizeof(InputBuffer) - 1] = '\0';

				if (ImGui::InputText(std::format("##PropertyName{}", Index).c_str(), InputBuffer, sizeof(InputBuffer),
					ImGuiInputTextFlags_EnterReturnsTrue |
					ImGuiInputTextFlags_AutoSelectAll |
					ImGuiInputTextFlags_AlwaysOverwrite))
				{
					Sprite->Name = InputBuffer;
					EditingSprites[EditingSpriteName] = false; // save and exit editing mode
				}

				// If you press ESC, you cancel editing
				if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					EditingSprites[EditingSpriteName] = false;
				}
				// Click outside the field - we also finish editing
				if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
				{
					Sprite->Name = InputBuffer;
					EditingSprites[EditingSpriteName] = false;
				}
				ImGui::SetItemDefaultFocus(); // the cursor is immediately in InputText

				if (!EditingSprites[EditingSpriteName])
				{
					FEvent_Sprite Event;
					Event.Tag = FEventTag::RenamedSpriteTag;
					Event.SpriteName = Sprite->Name;
					Event.UniqueID = Sprite->UniqueID;
					SendEvent(Event);
				}
			}
			else
			{
				const std::string SpriteName = std::format("{}##{}_SpriteName{}", Sprite->Name.c_str(), Sprites[Index]->StaticUniqueID, SpriteNum);
				if (ImGui::Selectable(SpriteName.c_str(), bIsSelectedSprite, 0, ImVec2(PrevSize.x, 0)))
				{
					IndexSelectedSprite = Index;
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					EditingSprites[EditingSpriteName] = true;
				}
			}
			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();

		const ImGuiIO& IO = ImGui::GetIO();
		if (bHovered && IO.MouseReleased[ImGuiMouseButton_Right])
		{
			ImGui::OpenPopup(PopupMenu_EditMetadataID);
			IndexSelectedSprite = Index;
		}

		const float LastButton_x2 = ImGui::GetItemRectMax().x;
		const float NextButton_x2 = LastButton_x2 + Style.ItemSpacing.x + VisibleSize.x; // Expected position if next button was on same line
		if (Index + 1 < SpriteNum && NextButton_x2 < WindowVisible_x2)
		{
			ImGui::SameLine();
		}
		ImGui::PopID();
	}

	ImGui::PopStyleVar(3);
}

void SSpriteList::Draw_ExportSprites()
{
	static bool bOpenFileDialog = false;
	const float TextWidth = ImGui::CalcTextSize("A").x;
	const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

	ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
	ImGui::SeparatorText("Path");
	ImGui::Text("%s", std::filesystem::absolute(CurrentPath).string().c_str());
	ImGui::SameLine(512.0f);
	if (ImGui::Button("...", ImVec2(0.0f, 0.0f)))
	{
		bOpenFileDialog = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));
	ImGui::SeparatorText("Script");

	if (ImGui::IsWindowAppearing())
	{
		ScriptFileNames.clear();
		ScriptFileNames.reserve(ScriptFiles.size());
		std::transform(
			ScriptFiles.begin(), ScriptFiles.end(),
			std::back_inserter(ScriptFileNames),
			[](const auto& pair) { return pair.first; }
		);
	}

	if (ImGui::BeginCombo("##ScriptCombo", IndexSelectedScript == INDEX_NONE ? "None" : ScriptFileNames[IndexSelectedScript].c_str(), 0))
	{
		static ImGuiTextFilter Filter;
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
			Filter.Clear();
		}
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
		Filter.Draw("##Filter", -FLT_MIN);

		for (int32_t Index = 0; Index < ScriptFileNames.size(); ++Index)
		{
			const bool bIsSelected = (IndexSelectedScript == Index);
			if (Filter.PassFilter(ScriptFileNames[Index].c_str()))
			{
				if (ImGui::Selectable(ScriptFileNames[Index].c_str(), bIsSelected))
				{
					IndexSelectedScript = Index;
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));
	ImGui::Checkbox("use a unique file name ##UniqueFilename", &bUniqueExportFilename);
	if (ImGui::ButtonEx("OK", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
	{
		// export sprites
		{
			std::vector<std::shared_ptr<FSprite>> SelectedSprites;
			for (const std::shared_ptr<FSprite>& Sprite : Sprites)
			{
				if (!Sprite->bSelected)
				{
					continue;
				}
				SelectedSprites.push_back(Sprite);
			}

			const std::string ScriptFileName = IndexSelectedScript == INDEX_NONE ? "" : ScriptFileNames[IndexSelectedScript];
			ExportSprites(ScriptFiles[ScriptFileName], CurrentPath, SelectedSprites);
		}

		bNeedKeptOpened_ExportPopup = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SetItemDefaultFocus();
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
	{
		bNeedKeptOpened_ExportPopup = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();

	if (bOpenFileDialog)
	{
		bOpenFileDialog = false;
		SFileDialog::OpenWindow(GetParent(), "Select File", EDialogMode::Select,
			[this](std::filesystem::path SelectedPath, EDialogStage Selected) -> void
			{
				if (Selected == EDialogStage::Selected)
				{
					CurrentPath = SelectedPath.string();
				}
				SFileDialog::CloseWindow();
				bNeedKeptOpened_ExportPopup = true;
			}, CurrentPath, "*.*");
	}
}

void SSpriteList::AddSprite(
	const ImRect& SpriteRect,
	const std::string& Name,
	const std::filesystem::path& SourcePathFile,
	int32_t Width, int32_t Height,
	const std::vector<uint8_t>& IndexedData,
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData)
{
	std::shared_ptr<FSprite> NewSprite = std::make_shared<FSprite>();
	NewSprite->bSelected = false;
	NewSprite->HoverStartTime = -1.0;
	NewSprite->Width = (uint32_t)SpriteRect.GetWidth();
	NewSprite->Height = (uint32_t)SpriteRect.GetHeight();
	NewSprite->Name = Name;

	NewSprite->SpritePositionToImageX = (uint32_t)SpriteRect.GetTL().x;
	NewSprite->SpritePositionToImageY = (uint32_t)SpriteRect.GetTL().y;
	NewSprite->SourcePathFile = SourcePathFile;

	NewSprite->ZXColorView = std::make_shared<UI::FZXColorView>();
	NewSprite->ZXColorView->Scale = ImVec2(1.0f, 1.0f);
	NewSprite->ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

	{
		FEvent_Sprite Event;
		Event.Tag = FEventTag::RenamedSpriteTag;
		Event.SpriteName = NewSprite->Name;
		Event.UniqueID = NewSprite->UniqueID;
		SendEvent(Event);
	}

	{
		const uint32_t NormalSizeX = uint32_t(NewSprite->Width / 8) * 8;
		const uint32_t NormalSizeY = uint32_t(NewSprite->Height / 8) * 8;

		if (NormalSizeX != NewSprite->Width || NormalSizeY != NewSprite->Height)
		{
			// ToDo: convert index color to ZX format
			return;
		}

		const int32_t Size = NewSprite->Width * NewSprite->Height;
		std::vector<uint32_t> RGBA(Size);

		std::vector<uint8_t>& NewIndexedData = NewSprite->ZXColorView->IndexedData;
		NewIndexedData.resize(Size);

		const int32_t Boundary_X = Width >> 3;
		const int32_t Boundary_Y = Height >> 3;
		const int32_t SpriteBoundary_X = NormalSizeX >> 3;
		const int32_t SpriteBoundary_Y = NormalSizeY >> 3;

		const int32_t PixelSize = SpriteBoundary_X * NewSprite->Height;
		std::vector<uint8_t>& NewInkData = NewSprite->ZXColorView->InkData;
		NewInkData.resize(PixelSize);
		std::vector<uint8_t>& NewMaskData = NewSprite->ZXColorView->MaskData;
		NewMaskData.resize(PixelSize);

		const int32_t AttributeSize = SpriteBoundary_X * SpriteBoundary_Y;
		std::vector<uint8_t>& NewAttributeData = NewSprite->ZXColorView->AttributeData;
		NewAttributeData.resize(AttributeSize);

		uint32_t Index = 0;
		for (uint32_t y = (uint32_t)SpriteRect.Min.y; y < (uint32_t)SpriteRect.Max.y; ++y)
		{
			for (uint32_t x = (uint32_t)SpriteRect.Min.x; x < (uint32_t)SpriteRect.Max.x; ++x)
			{
				const int32_t bx = x / 8;
				const int32_t dx = x % 8;
				const int32_t by = y / 8;
				const int32_t dy = y % 8;

				const int32_t Sprbx = (x - (uint32_t)SpriteRect.Min.x) / 8;
				const int32_t Sprby = (y - (uint32_t)SpriteRect.Min.y) / 8;
				const int32_t Sprdy = (y - (uint32_t)SpriteRect.Min.y) % 8;

				uint8_t Mask = 0xFF;
				uint8_t Pixels = 0x00;
				uint8_t InkColor = UI::EZXSpectrumColor::Black_;
				uint8_t PaperColor = UI::EZXSpectrumColor::White_;

				// Ink
				{
					const int32_t InkOffset = (by * 8 + dy) * Boundary_X + bx;
					Pixels = InkData[InkOffset];

					const int32_t NewInkOffset = (Sprby * 8 + Sprdy) * SpriteBoundary_X + Sprbx;
					NewInkData[NewInkOffset] = Pixels;
				}

				// Mask
				{
					const int32_t MaskOffset = (by * 8 + dy) * Boundary_X + bx;
					Mask = MaskData[MaskOffset];
					const int32_t NewMaskOffset = (Sprby * 8 + Sprdy) * SpriteBoundary_X + Sprbx;
					NewMaskData[NewMaskOffset] = Mask;
					Mask = ~Mask;
				}

				// Attribute
				{
					const int32_t AttributeOffset = by * Boundary_X + bx;
					const uint8_t Attribute = AttributeData[AttributeOffset];

					const int32_t NewAttributeOffset = Sprby * SpriteBoundary_X + Sprbx;
					NewAttributeData[NewAttributeOffset] = Attribute;

					const bool bBright = (Attribute >> 6) & 0x01;
					InkColor = (Attribute & 0x07) | (bBright << 3);
					PaperColor = ((Attribute >> 3) & 0x07) | (bBright << 3);
				}

				if (InkColor == UI::EZXSpectrumColor::Transparent)
				{
					InkColor = UI::EZXSpectrumColor::Black_;
				}
				if (PaperColor == UI::EZXSpectrumColor::Transparent)
				{
					PaperColor = UI::EZXSpectrumColor::Black_;
				}

				const int8_t ColorInk = (Pixels << dx) & 0x80 ? InkColor : PaperColor;
				const int8_t Color = ((Mask << dx) & 0x80) ? UI::EZXSpectrumColor::Transparent : ColorInk;
				NewIndexedData[Index] = Color;

				const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[Color]);
				RGBA[Index] = ColorRGBA;

				++Index;
			}
		}

		FImageBase& Images = FImageBase::Get();
		NewSprite->ZXColorView->Image = Images.CreateTexture(RGBA.data(), NewSprite->Width, NewSprite->Height, D3D11_CPU_ACCESS_READ, D3D11_USAGE_DEFAULT);
	}
	
	NewSprite->ZXColorView->Device = Data.Device;
	NewSprite->ZXColorView->DeviceContext = Data.DeviceContext;
	Draw_ZXColorView_Initialize(NewSprite->ZXColorView, UI::ERenderType::Sprite);

	Sprites.push_back(NewSprite);
}

std::vector<std::shared_ptr<FSprite>> SSpriteList::UpdateSprite(const std::filesystem::path& SourcePathFile, const std::vector<uint8_t>& IndexedData, const std::vector<uint8_t>& InkData, const std::vector<uint8_t>& AttributeData, const std::vector<uint8_t>& MaskData)
{
	std::vector<std::shared_ptr<FSprite>> UpdatedSprites;

	for (std::shared_ptr<FSprite>& Sprite : Sprites)
	{
		if (Sprite->SourcePathFile != SourcePathFile)
		{
			continue;
		}

		const int32_t Boundary_X = Sprite->Width >> 3;
		const int32_t Boundary_Y = Sprite->Height >> 3;
		const uint32_t NormalSizeX = uint32_t(Sprite->Width / 8) * 8;
		const uint32_t NormalSizeY = uint32_t(Sprite->Height / 8) * 8;
		const int32_t SpriteBoundary_X = NormalSizeX >> 3;
		const int32_t SpriteBoundary_Y = NormalSizeY >> 3;

		const int32_t Size = Sprite->Width * Sprite->Height;
		std::vector<uint32_t> RGBA(Size);

		uint32_t Index = 0;
		for (uint32_t y = Sprite->SpritePositionToImageY; y < (Sprite->SpritePositionToImageY + Sprite->Height); ++y)
		{
			for (uint32_t x = Sprite->SpritePositionToImageX; x < (Sprite->SpritePositionToImageX + Sprite->Width); ++x)
			{
				const int32_t bx = x / 8;
				const int32_t dx = x % 8;
				const int32_t by = y / 8;
				const int32_t dy = y % 8;

				const int32_t Sprbx = (x - Sprite->SpritePositionToImageX) / 8;
				const int32_t Sprby = (y - Sprite->SpritePositionToImageY) / 8;
				const int32_t Sprdy = (y - Sprite->SpritePositionToImageY) % 8;

				uint8_t Mask = 0xFF;
				uint8_t Pixels = 0x00;
				uint8_t InkColor = UI::EZXSpectrumColor::Black_;
				uint8_t PaperColor = UI::EZXSpectrumColor::White_;

				// Ink
				{
					const int32_t InkOffset = (by * 8 + dy) * Boundary_X + bx;
					Pixels = InkData[InkOffset];

					const int32_t NewInkOffset = (Sprby * 8 + Sprdy) * SpriteBoundary_X + Sprbx;
					Sprite->ZXColorView->InkData[InkOffset] = Pixels;
				}

				// Mask
				{
					const int32_t MaskOffset = (by * 8 + dy) * Boundary_X + bx;
					Mask = MaskData[MaskOffset];
					const int32_t NewMaskOffset = (Sprby * 8 + Sprdy) * SpriteBoundary_X + Sprbx;
					Sprite->ZXColorView->MaskData[NewMaskOffset] = Mask;
					Mask = ~Mask;
				}

				// Attribute
				{
					const int32_t AttributeOffset = by * Boundary_X + bx;
					const uint8_t Attribute = AttributeData[AttributeOffset];

					const int32_t NewAttributeOffset = Sprby * SpriteBoundary_X + Sprbx;
					Sprite->ZXColorView->AttributeData[NewAttributeOffset] = Attribute;

					const bool bBright = (Attribute >> 6) & 0x01;
					InkColor = (Attribute & 0x07) | (bBright << 3);
					PaperColor = ((Attribute >> 3) & 0x07) | (bBright << 3);
				}

				if (InkColor == UI::EZXSpectrumColor::Transparent)
				{
					InkColor = UI::EZXSpectrumColor::Black_;
				}
				if (PaperColor == UI::EZXSpectrumColor::Transparent)
				{
					PaperColor = UI::EZXSpectrumColor::Black_;
				}

				const int8_t ColorInk = (Pixels << dx) & 0x80 ? InkColor : PaperColor;
				const int8_t Color = ((Mask << dx) & 0x80) ? UI::EZXSpectrumColor::Transparent : ColorInk;
				Sprite->ZXColorView->IndexedData[Index] = Color;

				const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[Color]);
				RGBA[Index] = ColorRGBA;

				++Index;
			}
		}

		FImageBase& Images = FImageBase::Get();
		Images.UpdateTexture(Sprite->ZXColorView->Image.Handle, RGBA.data());

		UpdatedSprites.push_back(Sprite);
	}

	return std::move(UpdatedSprites);
}

bool SSpriteList::ImportSprites(const std::filesystem::path& FilePath, std::vector<std::shared_ptr<FSprite>>& OutputSprites)
{
	nlohmann::ordered_json Json;
	std::ifstream JsonFile(FilePath, std::ios::binary);
	if (!JsonFile.is_open())
	{
		return false;
	}

	try
	{
		JsonFile >> Json;
		JsonFile.close();
	}
	catch (const nlohmann::json::parse_error& e)
	{
		LOG("JSON parsing error: {}", e.what());
		return false;
	}
	
	auto FromUtf8 = [](const std::string& str) -> std::wstring
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
			return conv.from_bytes(str);
		};

	OutputSprites.clear();
	for (const auto& SpriteJson : Json)
	{
		std::shared_ptr<FSprite> NewSprite = std::make_shared<FSprite>();
		NewSprite->bSelected = false;
		NewSprite->HoverStartTime = -1.0;

		NewSprite->ZXColorView = std::make_shared<UI::FZXColorView>();
		NewSprite->ZXColorView->Scale = ImVec2(1.0f, 1.0f);
		NewSprite->ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);
		NewSprite->ZXColorView->Device = Data.Device;
		NewSprite->ZXColorView->DeviceContext = Data.DeviceContext;
		Draw_ZXColorView_Initialize(NewSprite->ZXColorView, UI::ERenderType::Sprite);

		// main parameters
		NewSprite->Name = SpriteJson.value("SprName", "");
		NewSprite->Width = SpriteJson.value("SprWidth", 0);
		NewSprite->Height = SpriteJson.value("SprHeight", 0);
		NewSprite->SpritePositionToImageX = SpriteJson.value("PoxImgX", 0);
		NewSprite->SpritePositionToImageY = SpriteJson.value("PoxImgY", 0);
		NewSprite->SourcePathFile = FromUtf8(SpriteJson.value("FileImg", ""));

		// data files
		std::filesystem::path InkDataFile = FromUtf8(SpriteJson.value("InkData", ""));
		std::filesystem::path AttributeDataFile = FromUtf8(SpriteJson.value("AttributeData", ""));
		std::filesystem::path MaskDataFile = FromUtf8(SpriteJson.value("MaskData", ""));

		// reading binary data
		if (std::filesystem::exists(InkDataFile))
		{
			IO::LoadBinaryData(NewSprite->ZXColorView->InkData, InkDataFile);
		}
		if (std::filesystem::exists(AttributeDataFile))
		{
			IO::LoadBinaryData(NewSprite->ZXColorView->AttributeData, AttributeDataFile);
		}
		if (std::filesystem::exists(MaskDataFile))
		{
			IO::LoadBinaryData(NewSprite->ZXColorView->MaskData, MaskDataFile);
		}

		// additional data
		if (SpriteJson.contains("Regions"))
		{
			NewSprite->Regions = SpriteJson["Regions"];

			for (FSpriteMetaRegion& Region : NewSprite->Regions)
			{
				Region.ZXColorView = std::make_shared<UI::FZXColorView>();
				Region.ZXColorView->bOnlyNearestSampling = true;
				Region.ZXColorView->Device = Data.Device;
				Region.ZXColorView->DeviceContext = Data.DeviceContext;
				UI::Draw_ZXColorView_Initialize(Region.ZXColorView, UI::ERenderType::Sprite);
				{
					const int32_t Size = NewSprite->Width * NewSprite->Height;
					std::vector<uint32_t> RGBA(Size, 0);

					if (!Region.ZXColorView->Image.IsValid())
					{
						for (uint32_t y = (uint32_t)Region.Rect.Min.y; y < (uint32_t)Region.Rect.Max.y; ++y)
						{
							for (uint32_t x = (uint32_t)Region.Rect.Min.x; x < (uint32_t)Region.Rect.Max.x; ++x)
							{
								const int8_t Color = UI::EZXSpectrumColor::Black_;
								const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[Color]);

								const uint32_t Index = y * NewSprite->Width + x;
								RGBA[Index] = ColorRGBA;
							}
						}
						Region.ZXColorView->Image = FImageBase::Get().CreateTexture(RGBA.data(), NewSprite->Width, NewSprite->Height, D3D11_CPU_ACCESS_READ, D3D11_USAGE_DEFAULT);
					}
				}
			}
		}
		UI::ZXAttributeColorToImage(
			NewSprite->ZXColorView->Image,
			NewSprite->Width, NewSprite->Height,
			NewSprite->ZXColorView->InkData.data(),
			NewSprite->ZXColorView->AttributeData.data(),
			NewSprite->ZXColorView->MaskData.data(),
			true,
			&NewSprite->ZXColorView->IndexedData, true /* ????????!!!!!!!*/);
		OutputSprites.push_back(NewSprite);
	}

	return true;
}

void SSpriteList::ExportSprites(const std::filesystem::path& ScriptFilePath, const std::filesystem::path& ExportPath, const std::vector<std::shared_ptr<FSprite>>& SelectedSprites, bool bOpenFolder /*= true*/)
{
	std::error_code ec;
	nlohmann::ordered_json Json;

	if (!ScriptFilePath.empty())
	{
		try
		{
			std::filesystem::copy_file(ScriptFilePath, IO::NormalizePath(ExportPath / ScriptFilePath.filename()), std::filesystem::copy_options::overwrite_existing);
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			std::cerr << "Error copying file: " << e.what() << std::endl;
			std::cerr << "Source path: " << e.path1() << "\n";
			std::cerr << "Destination path: " << e.path2() << "\n";
		}
	}

	auto ToUtf8 = [](const std::wstring& wstr) -> std::string
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
			return conv.to_bytes(wstr);
		};

	for (const std::shared_ptr<FSprite>& Sprite : SelectedSprites)
	{
		std::filesystem::path IndexedDataFilePath;
		if (Sprite->ZXColorView->IndexedData.size() > 0)
		{
			IndexedDataFilePath = IO::NormalizePath(ExportPath / std::format("{}.idx.png", Sprite->Name));
			const std::filesystem::path UniqueIndexedDataFilePath = bUniqueExportFilename ? IO::GetUniquePath(IndexedDataFilePath, ec) : IndexedDataFilePath;
			if (ec)
			{
				return;
			}

			constexpr int32_t Channels = 4;
			std::vector<uint32_t> RGBA(Sprite->Width * Sprite->Height);
			{
				for (uint32_t y = 0; y < Sprite->Height; ++y)
				{
					for (uint32_t x = 0; x < Sprite->Width; ++x)
					{
						const uint32_t Offset = y * Sprite->Width + x;
						const uint8_t ColorIndex = Sprite->ZXColorView->IndexedData[Offset];
						const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[ColorIndex]);
						RGBA[Offset] = ColorRGBA;
					}
				}
			}

			if (!stbi_write_png(
				UniqueIndexedDataFilePath.string().c_str(),
				Sprite->Width,
				Sprite->Height,
				Channels,
				RGBA.data(),
				Sprite->Width * Channels
			))
			{
				std::cerr << "Failed to write PNG!" << std::endl;
				LOG_ERROR("[{}]\t Failed to write PNG!", (__FUNCTION__));
			}
		}

		std::filesystem::path InkDataFilePath;
		if (Sprite->ZXColorView->InkData.size() > 0)
		{
			InkDataFilePath = IO::NormalizePath(std::filesystem::absolute(ExportPath / std::format("{}.ink.bin", Sprite->Name)));
			IO::SaveBinaryData(Sprite->ZXColorView->InkData, InkDataFilePath, bUniqueExportFilename);
		}

		std::filesystem::path AttributeDataFilePath;
		if (Sprite->ZXColorView->AttributeData.size() > 0)
		{
			AttributeDataFilePath = IO::NormalizePath(std::filesystem::absolute(ExportPath / std::format("{}.attr.bin", Sprite->Name)));
			IO::SaveBinaryData(Sprite->ZXColorView->AttributeData, AttributeDataFilePath, bUniqueExportFilename);
		}

		std::filesystem::path MaskDataFilePath;
		if (Sprite->ZXColorView->MaskData.size() > 0)
		{
			MaskDataFilePath = IO::NormalizePath(std::filesystem::absolute(ExportPath / std::format("{}.mask.bin", Sprite->Name)));
			IO::SaveBinaryData(Sprite->ZXColorView->MaskData, MaskDataFilePath, bUniqueExportFilename);
		}

		nlohmann::ordered_json SpriteJson =
			{
				{"SprName", Sprite->Name},
				{"SprWidth", Sprite->Width},
				{"SprHeight", Sprite->Height},
				{"PoxImgX", Sprite->SpritePositionToImageX},
				{"PoxImgY", Sprite->SpritePositionToImageY},
				{"FileImg", ToUtf8(IO::NormalizePath(Sprite->SourcePathFile).wstring())},

				{"InkData", ToUtf8(InkDataFilePath.wstring())},
				{"AttributeData", ToUtf8(AttributeDataFilePath.wstring())},
				{"MaskData", ToUtf8(MaskDataFilePath.wstring())},
				{"Regions", Sprite->Regions },
			};

		Json.push_back(SpriteJson);
	}

	std::filesystem::path JsonFilePath = IO::NormalizePath(ExportPath / "Export.json");
	const std::filesystem::path UniqueJsonFilePath = bUniqueExportFilename ? IO::GetUniquePath(JsonFilePath, ec) : JsonFilePath;
	if (ec)
	{
		return;
	}
	std::ofstream JsonFile(UniqueJsonFilePath, std::ios::binary);
	if (JsonFile.is_open())
	{
		JsonFile << Json.dump(4);
		JsonFile.close();
	}
	if (bOpenFolder)
	{
		IO::OpenFolder(ExportPath);
	}
}

void SSpriteList::SendSelectedSprite() const 
{
	if (IndexSelectedSprite != INDEX_NONE && Sprites.size() > IndexSelectedSprite)
	{
		// forced open window 'Edit metadata'
		{
			std::shared_ptr<SViewerBase> Viewer = GetParent();
			if (Viewer && !Viewer->IsWindowVisibility(NAME_SpriteMetadata))
			{
				Viewer->SetWindowVisibility(NAME_SpriteMetadata);
			}
		}

		FEvent_SelectedSprite Event;
		{
			Event.Tag = FEventTag::SelectedSpritesChangedTag;
			Event.Sprites.push_back(Sprites[IndexSelectedSprite]);
		}
		SendEvent(Event);
	}
}

void SSpriteList::ApplyImportSprites(const std::vector<std::shared_ptr<FSprite>>& ReadSprites)
{
	Sprites = ReadSprites;
	std::shared_ptr<SViewerBase> Viewer = GetParent();
	for (std::shared_ptr<FSprite> Sprite : Sprites)
	{
		const std::wstring Filename = Sprite->SourcePathFile.filename().wstring();
		if (!Viewer->GetWindow(NAME_Canvas, Filename))
		{
			FNativeDataInitialize _Data
			{
				.Device = Data.Device,
				.DeviceContext = Data.DeviceContext
			};

			std::wstring Filename = Sprite->SourcePathFile.filename().wstring();
			std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename, Sprite->SourcePathFile);
			Viewer->AddWindow(EName::Canvas, NewCanvas, _Data, { Sprite->SourcePathFile, EImageFormat::PNG });
		}
		else
		{
			Viewer->SetWindowVisibility(NAME_Canvas, true, Filename);
		}
	}
}
