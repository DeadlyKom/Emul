#include "SpriteMetadata.h"
#include <Window/Sprite/Events.h>
#include "SpriteList.h"

#include <Utils/Array.h>
#include <Utils/UI/Draw.h>

namespace
{
	static const wchar_t* ThisWindowName = L"Sprite Metadata";
}

SSpriteMetadata::SSpriteMetadata(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetOpen(false)
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(false))
{}

void SSpriteMetadata::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_SelectedSprite>(
		[this](const FEvent_SelectedSprite& Event)
		{
			if (Event.Tag == FEventTag::SelectedSpritesChangedTag)
			{
				SelectedSprite = Event.Sprites.empty() ? nullptr : Event.Sprites[0];
			}
		});
}

void SSpriteMetadata::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen, ImGuiWindowFlags_NoResize);
	{
		if (SelectedSprite)
		{
			Draw_Metadata();
		}
		ImGui::End();
	}

	if (!IsOpen())
	{
		EditingProperty.clear();
	}
}

void SSpriteMetadata::Tick(float DeltaTime)
{
	HeverTooltip += DeltaTime;
}

void SSpriteMetadata::Destroy()
{
	
	UnsubscribeAll();
}

void SSpriteMetadata::Draw_Metadata()
{
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetWindowSize(ImVec2(850.0f, 420.0f));
		ImGui::SetWindowCollapsed(false);
		IndexSelectedRegion = INDEX_NONE;
		IndexSelectedProperty = INDEX_NONE;
	}

	ImVec2 RegionSize, SpriteSize, PropertySize, ButtonPosition;
	const float TextWidth = ImGui::CalcTextSize("A").x;
	const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

	const ImVec2 Size = ImGui::GetWindowSize();
	ImGuiStyle& Style = ImGui::GetStyle();

	const float HeightButton = TextWidth * 5.0f;

	RegionSize.x = Size.x * 0.3f;
	RegionSize.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - HeightButton - TextWidth;

	SpriteSize.x = Size.x * 0.3f - Style.WindowPadding.x * 3;
	SpriteSize.y = SpriteSize.x;

	PropertySize.x = Size.x - RegionSize.x - SpriteSize.x - Style.WindowPadding.x * 5;
	PropertySize.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - HeightButton - TextWidth;

	ButtonPosition.x = RegionSize.x - ImGui::CalcTextSize(" + ").x - Style.WindowPadding.y * 3;
	ButtonPosition.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - TextWidth;

	Draw_Regions(RegionSize);
	ImGui::SameLine();
	Draw_Sprite(SpriteSize);
	ImGui::SameLine();
	Draw_Property(PropertySize);	
	ImGui::SameLine();
	Draw_Buttons(ButtonPosition, HeightButton);
}

void SSpriteMetadata::Draw_Regions(const ImVec2& Size)
{
	if (ImGui::BeginChild("Meta", Size, true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (int32_t RegionIndex = 0; RegionIndex < SelectedSprite->Regions.size(); ++RegionIndex)
		{
			const bool bIsSelectedRegion = RegionIndex == IndexSelectedRegion;
			FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[RegionIndex];

			if (ImGui::CollapsingHeader(
				std::format(" {} Region #{} {}##Region", bIsSelectedRegion ? ">" : "", RegionIndex, bIsSelectedRegion ? "<" : "").c_str(),
				&SpriteMetaRegion.bVisible,
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanFullWidth |
				(bIsSelectedRegion ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None)))
			{
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Region info:");
					ImGui::Spacing();
					ImGui::TextUnformatted(std::format(" - size: {} x {}", SpriteMetaRegion.Rect.GetSize().x, SpriteMetaRegion.Rect.GetSize().y).c_str());
					ImGui::EndTooltip();
				}

				for (int32_t PropertyIndex = 0; PropertyIndex < SpriteMetaRegion.Properties.size(); ++PropertyIndex)
				{
					const std::string EditingPropertName = std::format("{}_PropertyName{}", RegionIndex, PropertyIndex);

					FSpriteProperty& Property = SpriteMetaRegion.Properties[PropertyIndex];
					const bool bIsSelectedProperty = bIsSelectedRegion && PropertyIndex == IndexSelectedProperty;

					// Enable editing by pressing F2
					if (bIsSelectedProperty && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
					{
						EditingProperty[EditingPropertName] = true;
					}

					// Enable double-click editing
					if (EditingProperty[EditingPropertName])
					{
						ImGui::SetKeyboardFocusHere();

						char InputBuffer[128];
						std::strncpy(InputBuffer, Property.Name.c_str(), sizeof(InputBuffer));
						InputBuffer[sizeof(InputBuffer) - 1] = '\0';

						if (ImGui::InputText(std::format("##PropertyName{}", PropertyIndex).c_str(), InputBuffer, sizeof(InputBuffer),
							ImGuiInputTextFlags_EnterReturnsTrue |
							ImGuiInputTextFlags_AutoSelectAll |
							ImGuiInputTextFlags_AlwaysOverwrite))
						{
							Property.Name = InputBuffer;
							EditingProperty[EditingPropertName] = false; // save and exit editing mode
						}

						// If you press ESC, you cancel editing
						if (ImGui::IsKeyPressed(ImGuiKey_Escape))
						{
							EditingProperty[EditingPropertName] = false;
						}
						// Click outside the field - we also finish editing
						if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
						{
							EditingProperty[EditingPropertName] = false;
						}
						ImGui::SetItemDefaultFocus(); // the cursor is immediately in InputText
					}
					else
					{
						const std::string PropertyName = std::format("{}##{}_PropertyName{}", Property.Name.c_str(), RegionIndex, PropertyIndex);
						if (ImGui::Selectable(PropertyName.c_str(), bIsSelectedProperty))
						{
							IndexSelectedProperty = PropertyIndex;
						}

						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
						{
							EditingProperty[EditingPropertName] = true;
						}
					}
				}
			}

			if (!SpriteMetaRegion.bVisible)
			{
				if (RemoveAtSwap(SelectedSprite->Regions, RegionIndex))
				{
					--RegionIndex;
				}
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted("Region info:");
				ImGui::Spacing();
				ImGui::TextUnformatted(std::format(" - size: {} x {}", SpriteMetaRegion.Rect.GetSize().x, SpriteMetaRegion.Rect.GetSize().y).c_str());
				ImGui::EndTooltip();
			}

			if (ImGui::IsItemClicked()/* && !ImGui::IsItemToggledOpen()*/)
			{
				IndexSelectedRegion = RegionIndex;
			}
		}
	}
	ImGui::EndChild();
}

void SSpriteMetadata::Draw_Sprite(const ImVec2& Size)
{
	UI::FButtonZXColorSpriteSettings::FLayer Layers = {};
	if (IndexSelectedRegion != INDEX_NONE && SelectedSprite->Regions.size() > IndexSelectedRegion)
	{
		FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[IndexSelectedRegion];
		Layers = { SpriteMetaRegion.ZXColorView };
	}
	UI::FButtonZXColorSpriteSettings Settings
	{
		.bHovered = false,
		.bVisibleAdvencedTooltip = false,
		.Layers = Layers,
	};
	UI::Draw_ButtonZXColorSprite("Sprite", SelectedSprite, Size, Settings);
}

void SSpriteMetadata::Draw_Property(const ImVec2& Size)
{
	if (IndexSelectedRegion == INDEX_NONE ||
		IndexSelectedRegion >= SelectedSprite->Regions.size() ||
		IndexSelectedProperty == INDEX_NONE ||
		IndexSelectedProperty >= SelectedSprite->Regions[IndexSelectedRegion].Properties.size())
	{
		return;
	}

	FSpriteMetaRegion& Region = SelectedSprite->Regions[IndexSelectedRegion];
	FSpriteProperty& Property = Region.Properties[IndexSelectedProperty];

	if (ImGui::BeginChild("Property", Size, true, ImGuiWindowFlags_HorizontalScrollbar))
	{	
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 0.5f));
		int32_t IndexSelectedMetaType = Property.IndexByType();
		ImGui::SeparatorText("Type : ");
		ImGui::PushItemWidth(100.0f);
		if (ImGui::Combo("###ItemType", &IndexSelectedMetaType, FSpriteProperty::TypeNames, IM_ARRAYSIZE(FSpriteProperty::TypeNames), IM_ARRAYSIZE(FSpriteProperty::TypeNames)))
		{
			Property.TypeByIndex(IndexSelectedMetaType);
		}
		ImGui::PopItemWidth();

		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 1.0f));
		ImGui::SeparatorText("Value : ");
		char ValueBuffer[128];
		snprintf(ValueBuffer, sizeof(ValueBuffer), "%s", Property.ToString().c_str());

		//ImGui::PushItemWidth(200.0f);
		if (ImGui::InputText(std::format("##Value{}_{}",IndexSelectedRegion, IndexSelectedProperty).c_str(), ValueBuffer, sizeof(ValueBuffer)))
		{
			try
			{
				std::string_view Buf(ValueBuffer);
				std::visit([&](auto&& Arg)
					{
						using T = std::decay_t<decltype(Arg)>;
						if constexpr (std::is_same_v<T, int>)
						{
							Arg = std::stoi(std::string(Buf));
						}
						else if constexpr (std::is_same_v<T, float>)
						{
							Arg = std::stof(std::string(Buf));
						}
						else if constexpr (std::is_same_v<T, bool>)
						{
							Arg = (Buf == "true");
						}
						else if constexpr (std::is_same_v<T, std::string>)
						{
							Arg = std::string(Buf);
						}
					}, Property.Variant);

				//if (std::holds_alternative<int>(Property.Variant))
				//{
				//	Property.Variant = std::stoi(ValueBuffer);
				//}
				//else if (std::holds_alternative<float>(Property.Variant))
				//{
				//	Property.Variant = std::stof(ValueBuffer);
				//}
				//else if (std::holds_alternative<std::string>(Property.Variant))
				//{
				//	Property.Variant = std::string(ValueBuffer);
				//}
				//else if (std::holds_alternative<bool>(Property.Variant))
				//{
				//	Property.Variant = (std::string(ValueBuffer) == "true");
				//}
			}
			catch (...)
			{
				// If the conversion failed, leave the old value
			}
		}
		//ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}

void SSpriteMetadata::Draw_Buttons(const ImVec2& Position, float HeightButton)
{
	ImGui::SetCursorPos(Position);
	const bool bAddRemovePropertyButtons = SelectedSprite->Regions.size() > IndexSelectedRegion && IndexSelectedRegion != INDEX_NONE;
	ImGui::BeginDisabled(!bAddRemovePropertyButtons);

	if (ImGui::ButtonEx("+", ImVec2(HeightButton * 0.5f, HeightButton * 0.5f)))
	{
		FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[IndexSelectedRegion];
		FSpriteProperty NewProperty(std::format("Property #{}", SpriteMetaRegion.Properties.size()).c_str(), false);
		SpriteMetaRegion.Properties.push_back(NewProperty);
	}

	bool bHovered = false;
	bHovered |= ImGui::IsItemHovered();
	if (ImGui::IsItemHovered() && HeverTooltip > 0.5f)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("add property");
		ImGui::EndTooltip();
	}

	ImGui::SetCursorPos({ Position.x + HeightButton * 0.8f, Position.y});
	if (ImGui::ButtonEx("-", ImVec2(HeightButton * 0.5f, HeightButton * 0.5f)))
	{
		FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[IndexSelectedRegion];
		if (SpriteMetaRegion.Properties.size() > IndexSelectedProperty && IndexSelectedProperty != INDEX_NONE)
		{
			SpriteMetaRegion.Properties.erase(SpriteMetaRegion.Properties.begin() + IndexSelectedProperty);
		}
	}
	bHovered |= ImGui::IsItemHovered();
	if (ImGui::IsItemHovered() && HeverTooltip > 0.5f)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("remove property");
		ImGui::EndTooltip();
	}
	ImGui::EndDisabled();

	if (!bHovered)
	{
		HeverTooltip = 0.0f;
	}
}
