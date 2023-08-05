#include "GraphicsViewer.h"
#include <ImGuiSupport/ImGuiTexture.h>

#include "ZXGraphicsView.h"
#include "../SpectrumEmu.h"
#include "../GameConfig.h"
#include <algorithm>
#include "CodeAnalyser/UI/CodeAnalyserUI.h"

#include "misc/cpp/imgui_stdlib.h"
#include <Util/Misc.h>


// Graphics Viewer
static int kGraphicsViewerWidth = 256;
static int kGraphicsViewerHeight = 512;
static int kScreenViewerWidth = 256;
static int kScreenViewerHeight = 192;


bool InitGraphicsViewer(FGraphicsViewerState &state)
{
	state.pGraphicsView = new FZXGraphicsView(kGraphicsViewerWidth, kGraphicsViewerHeight);
	state.pScreenView = new FZXGraphicsView(kScreenViewerWidth, kScreenViewerHeight);

	return true;
}

void ShutdownGraphicsViewer(FGraphicsViewerState& state)
{
	delete state.pGraphicsView;
	delete state.pScreenView;
}

// speccy colour CLUT
static const uint32_t g_kColourLUT[8] =
{
	0xFF000000,     // 0 - black
	0xFFFF0000,     // 1 - blue
	0xFF0000FF,     // 2 - red
	0xFFFF00FF,     // 3 - magenta
	0xFF00FF00,     // 4 - green
	0xFFFFFF00,     // 5 - cyan
	0xFF00FFFF,     // 6 - yellow
	0xFFFFFFFF,     // 7 - white
};

uint16_t GetAddressOffsetFromPositionInView(const FGraphicsViewerState &viewerState, int x,int y)
{
	const int xSizeChars = viewerState.XSizePixels >> 3;
	const FCodeAnalysisState& state = viewerState.pEmu->CodeAnalysis;
	const int kHorizontalDispCharCount = kGraphicsViewerWidth / 8;
	const FCodeAnalysisBank* pBank = state.GetBank(viewerState.Bank);
	const uint16_t addrInput = viewerState.AddressOffset;
	const int xCount = kHorizontalDispCharCount / xSizeChars;
	const int xSize = xCount * xSizeChars;
	const int xp = std::max(std::min(xSize, x / 8), 0);
	const int yp = std::max(std::min(kGraphicsViewerHeight, y), 0);
	const int column = xp / xSizeChars;
	const int columnSize = kGraphicsViewerHeight * xSizeChars;

	ImGui::Text("xp: %d, yp: %d, column: %d", xp, yp, column);
	return ((addrInput + xp) + (column * columnSize) + (y * xSizeChars)) % viewerState.MemorySize;
}

uint8_t GetHeatmapColourForMemoryAddress(const FCodeAnalysisPage& page, uint16_t addr, int currentFrameNo, int frameThreshold)
{
	const uint16_t pageAddress = addr & FCodeAnalysisPage::kPageMask;
	const FCodeInfo* pCodeInfo = page.CodeInfo[pageAddress];

	if (pCodeInfo)
	{
		const int framesSinceExecuted = currentFrameNo - pCodeInfo->FrameLastExecuted;
		if (pCodeInfo->FrameLastExecuted != -1 && (framesSinceExecuted < frameThreshold))
			return 6;	// yellow code
	}

	const FDataInfo& dataInfo = page.DataInfo[pageAddress];
	
	if (dataInfo.LastFrameWritten != -1)
	{
		const int framesSinceWritten = currentFrameNo - dataInfo.LastFrameWritten;
		if (framesSinceWritten < frameThreshold)
			return 2; // red
	}

	if (dataInfo.LastFrameRead != -1)
	{
		const int framesSinceRead = currentFrameNo - dataInfo.LastFrameRead;
		if (framesSinceRead < frameThreshold)
			return 4;	// green
	}	

	return 7;
}

void DrawMemoryBankAsGraphicsColumn(FGraphicsViewerState& viewerState, int16_t bankId, uint16_t memAddr, int xPos, int columnWidth)
{
	FZXGraphicsView* pGraphicsView = viewerState.pGraphicsView;
	FCodeAnalysisState& state = viewerState.pEmu->CodeAnalysis;
	FCodeAnalysisBank* pBank = state.GetBank(bankId);
	const uint16_t bankSizeMask = pBank->SizeMask;

	for (int y = 0; y < kGraphicsViewerHeight; y++)
	{
		for (int xChar = 0; xChar < columnWidth; xChar++)
		{
			const uint16_t bankAddr = memAddr & bankSizeMask;
			const uint8_t charLine = pBank->Memory[bankAddr];
			FCodeAnalysisPage& page = pBank->Pages[bankAddr >> FCodeAnalysisPage::kPageShift];
			const uint8_t col = GetHeatmapColourForMemoryAddress(page, memAddr, state.CurrentFrameNo,viewerState.HeatmapThreshold);
			pGraphicsView->DrawCharLine(charLine, xPos + (xChar * 8), y, col);

			memAddr++;
		}
	}
}

// WIP
// the idea is to store graphic sets that run sequentially in memory as these structures and have a viewer for them
struct FGraphicSet
{
	FAddressRef	Address;	// start address of images
	int			XSize;	// width in chars
	int			YSize;	// height in scanlines
	int			Count;	// number of images
};

// draw a graphic set to a graphics view
void DrawGraphicSetToView(FZXGraphicsView* pGraphicsView, const FCodeAnalysisState& state, const FGraphicSet& graphic)
{
}

void DrawCharacterGraphicsViewer(FGraphicsViewerState& viewerState);
void DrawScreenViewer(FGraphicsViewerState& viewerState);


// Viewer to view spectrum graphics
void DrawGraphicsViewer(FGraphicsViewerState& viewerState)
{
	if (ImGui::Begin("Graphics View"))
	{
		if (ImGui::BeginTabBar("GraphicsViewTabBar"))
		{
			if (ImGui::BeginTabItem("GFX"))
			{
				DrawCharacterGraphicsViewer(viewerState);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Screen"))
			{
				DrawScreenViewer(viewerState);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();

	}

	ImGui::End();
}

void DrawCharacterGraphicsViewer(FGraphicsViewerState& viewerState)
{
	FZXGraphicsView* pGraphicsView = viewerState.pGraphicsView;
	FCodeAnalysisState& state = viewerState.pEmu->CodeAnalysis;
	FCodeAnalysisBank* pBank = state.GetBank(viewerState.Bank);

	int byteOff = 0;
	const int kHorizontalDispCharCount = kGraphicsViewerWidth / 8;
	const int kVerticalDispPixCount = kGraphicsViewerHeight;

	// maybe find a better way to go between physical address space and banks
	if (ImGui::BeginCombo("Bank", GetBankText(state, viewerState.Bank)))
	{
		if (ImGui::Selectable(GetBankText(state, -1), viewerState.Bank == -1))
		{
			viewerState.Bank = -1;
			viewerState.MemorySize = 0x10000;	// 64K
		}
		const auto& banks = state.GetBanks();
		for (const auto& bank : banks)
		{
			if (ImGui::Selectable(GetBankText(state, bank.Id), viewerState.Bank == bank.Id))
			{
				FCodeAnalysisBank* pNewBank = state.GetBank(bank.Id);
				viewerState.Bank = bank.Id;
				viewerState.AddressOffset = 0;
				viewerState.MemorySize = pNewBank->GetSizeBytes();

			}
		}	

		ImGui::EndCombo();
	}

	ImGui::Combo("ViewMode", (int*)&viewerState.ViewMode, "Character\0CharacterWinding", (int)GraphicsViewMode::Count);

	// Address input
	int addrInput = pBank ? pBank->GetMappedAddress() + viewerState.AddressOffset : viewerState.AddressOffset;
	//ImGui::Text("viewerState Map Address: %s", NumStr((uint16_t)addrInput));
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	pGraphicsView->Draw();
	if (ImGui::IsItemHovered())
	{
		const int xp = (int)(io.MousePos.x - pos.x);// -(viewerState.XSizePixels / 2));
		const int yp = std::max((int)(io.MousePos.y - pos.y - (viewerState.YSizePixels/2)),0);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const int xChars = viewerState.XSizePixels >> 3;
		const int rx = (xp / viewerState.XSizePixels) * viewerState.XSizePixels;
		const int ry = yp;// ((yp / viewerState.YSizePixels) * viewerState.YSizePixels);
		const float rxp = pos.x + (float)rx;
		const float ryp = pos.y + (float)ry;
		dl->AddRect(ImVec2(rxp, ryp), ImVec2(rxp + (float)viewerState.XSizePixels, ryp + (float)viewerState.YSizePixels), 0xff00ffff);
		//const int addressOffset = (xp / 8) + (yp * (256 / 8));
		ImGui::BeginTooltip();
		const uint16_t gfxAddressOffset = GetAddressOffsetFromPositionInView(viewerState,rx, ry);
		FAddressRef ptrAddress;
		if (pBank != nullptr)
			ptrAddress = FAddressRef(pBank->Id, gfxAddressOffset + pBank->GetMappedAddress());
		else
			ptrAddress = state.AddressRefFromPhysicalAddress(gfxAddressOffset);
		
		if (ImGui::IsMouseDoubleClicked(0))
		{
			state.GetFocussedViewState().GoToAddress(ptrAddress);
			addrInput = pBank ? pBank->GetMappedAddress() + gfxAddressOffset : gfxAddressOffset;
		}
		if (ImGui::IsMouseClicked(0))
			viewerState.ClickedAddress = ptrAddress;

		ImGui::Text("%s", NumStr(ptrAddress.Address));
		ImGui::SameLine();
		DrawAddressLabel(state, state.GetFocussedViewState(), ptrAddress);
		ImGui::EndTooltip();
	}
	
	ImGui::SameLine();

	// simpler slider
	ImGui::VSliderInt("##int", ImVec2(64.0f, (float)kGraphicsViewerHeight), &addrInput, 0, 0xffff);

	/*static int kRowSize = kHorizontalDispCharCount * 8;
	int addrLine = addrInput / kRowSize;
	int addrOffset = addrInput % kRowSize;


	if (ImGui::VSliderInt("##int", ImVec2(64.0f, (float)kGraphicsViewerHeight), &addrLine, 0, 0xffff / kRowSize))
	{
		addrInput = (addrLine * kRowSize) + addrOffset;
	}
	if (ImGui::SliderInt("##offset", &addrOffset, 0, kRowSize -1))
	{
		addrInput = (addrLine * kRowSize) + addrOffset;
	}*/
	
	ImGui::SetNextItemWidth(100.0f);
	if (GetNumberDisplayMode() == ENumberDisplayMode::Decimal)
		ImGui::InputInt("##Address", &addrInput, 1, 8, ImGuiInputTextFlags_CharsDecimal);
	else
		ImGui::InputInt("##Address", &addrInput, 1, 8, ImGuiInputTextFlags_CharsHexadecimal);
	DrawAddressLabel(state, state.GetFocussedViewState(), state.AddressRefFromPhysicalAddress(addrInput));

	// put in config? 
	//ImGui::SliderInt("Heatmap frame threshold", &viewerState.HeatmapThreshold, 0, 60);
	pGraphicsView->Clear(0xff000000);

	FCodeAnalysisBank* pClickedBank = state.GetBank(viewerState.ClickedAddress.BankId);
	if(pClickedBank !=nullptr && state.Config.bShowBanks)
		ImGui::Text("Clicked Address: [%s]%s", pClickedBank->Name.c_str(), NumStr(viewerState.ClickedAddress.Address));
	else
		ImGui::Text("Clicked Address: %s", NumStr(viewerState.ClickedAddress.Address));
	//ImGui::SameLine();
	if (viewerState.ClickedAddress.IsValid())
	{
		DrawAddressLabel(state, state.GetFocussedViewState(), viewerState.ClickedAddress);
		if (ImGui::CollapsingHeader("Details"))
		{
			const int16_t bankId = viewerState.Bank != -1 ? viewerState.Bank : state.GetBankFromAddress(viewerState.ClickedAddress.Address);
			const FCodeAnalysisItem item(state.GetReadDataInfoForAddress(viewerState.ClickedAddress), viewerState.ClickedAddress);
			DrawDataDetails(state, state.GetFocussedViewState(), item);
		}
	}
	
	const int graphicsUnitSize = (viewerState.XSizePixels>>3) * viewerState.YSizePixels;

	//ImGui::Checkbox("Column Mode", &state.bColumnMode);
	if (ImGui::Button("<<"))
		addrInput -= graphicsUnitSize;
	ImGui::SameLine();
	if (ImGui::Button(">>"))
		addrInput += graphicsUnitSize;

	viewerState.AddressOffset = pBank != nullptr ? (int)addrInput - pBank->GetMappedAddress() : addrInput;

	// draw 64 * 8 bytes
	ImGui::InputInt("XSize", &viewerState.XSizePixels, 8, 8);
	ImGui::InputInt("YSize", &viewerState.YSizePixels, viewerState.YSizePixelsFineCtrl? 1:8, 8);
	ImGui::SameLine();
	ImGui::Checkbox("Fine", &viewerState.YSizePixelsFineCtrl);
	ImGui::InputInt("Count", &viewerState.ImageCount, 1, 1);
	//ImGui::InputInt("YSize Fine", &viewerState.YSizePixels, 1, 8);
#if 0
	ImGui::InputInt("Count", &viewerState.ImageCount, 1, 4);

	ImGui::Separator();
	ImGui::InputText("Config Name", &viewerState.NewConfigName);
	ImGui::SameLine();
	if (ImGui::Button("Store"))
	{
		// Store this in the config map
		auto& spriteConfigs = viewerState.pGame->pConfig->SpriteConfigs;
		if(spriteConfigs.find(viewerState.NewConfigName) == spriteConfigs.end())	// not found - add
		{
			FSpriteDefConfig newConfig;
			newConfig.BaseAddress = viewerState.AddressOffset;	// TODO: fix for banks
			newConfig.Count = viewerState.ImageCount;
			newConfig.Width = viewerState.XSize;
			newConfig.Height = viewerState.YSize / 8;	// sprite height in chars atm - TODO: move to line count
			spriteConfigs[viewerState.NewConfigName] = newConfig;

			// TODO: tell sprite view to refresh
			GenerateSpriteListsFromConfig(viewerState, viewerState.pGame->pConfig);

			// TODO: Save Config?
		}
			
	}
#endif
	viewerState.XSizePixels = std::min(std::max(8, viewerState.XSizePixels), kHorizontalDispCharCount * 8);
	viewerState.YSizePixels = std::min(std::max(1, viewerState.YSizePixels), kVerticalDispPixCount);

	const int xcount = kHorizontalDispCharCount / (viewerState.XSizePixels >> 3);
	const int ycount = kVerticalDispPixCount / viewerState.YSizePixels;

	int y = 0;
	int address = viewerState.AddressOffset;
	int xSizeChars = viewerState.XSizePixels >> 3;

	if (viewerState.ViewMode == GraphicsViewMode::Character)
	{
		for (int x = 0; x < xcount; x++)
		{
			int16_t bankId = viewerState.Bank;
			if (bankId == -1)
				bankId = state.GetBankFromAddress(address);

			assert(bankId != -1);
			DrawMemoryBankAsGraphicsColumn(viewerState, bankId, address & 0x3fff, x * viewerState.XSizePixels, xSizeChars);
			//	DrawMemoryAsGraphicsColumn(viewerState, address, x * viewerState.XSize * 8, viewerState.XSize);

			address += xSizeChars * kVerticalDispPixCount;
		}
	}
	else if (viewerState.ViewMode == GraphicsViewMode::CharacterWinding)
	{
		int offsetX = 0;
		int offsetY = 0;
		for (int y = 0; y < ycount; y++)
		{
			for (int x = 0; x < xcount; x++)
			{
				// draw single item
				for (int yLine = 0; yLine < viewerState.YSizePixels; yLine++)	// loop down scan lines
				{
					for (int xChar = 0; xChar < xSizeChars; xChar++)
					{
						const uint8_t* pImage = viewerState.pEmu->GetMemPtr(address);
						const int xp = ((yLine & 1) == 0) ? xChar : (xSizeChars - 1) - xChar;
						if (address + graphicsUnitSize < 0xffff)
							pGraphicsView->DrawCharLine(*pImage, offsetX + (xp * 8), offsetY + yLine);
						address++;
					}
				}

				offsetX += viewerState.XSizePixels;
			}
			offsetX = 0;
			offsetY += viewerState.YSizePixels;
		}
		address += graphicsUnitSize;
	}
}

// http://www.breakintoprogram.co.uk/computers/zx-spectrum/screen-memory-layout
void DrawScreenViewer(FGraphicsViewerState& viewerState)
{
	FZXGraphicsView* pGraphicsView = viewerState.pScreenView;
	const FCodeAnalysisState& state = viewerState.pEmu->CodeAnalysis;	
	const int16_t bankId = viewerState.Bank == -1 ? state.GetBankFromAddress(0x4000) : viewerState.Bank;
	const FCodeAnalysisBank* pBank = state.GetBank(bankId);

	uint16_t bankAddr = 0;
	for (int y = 0; y < 192; y++)
	{
		const int y0to2 = ((bankAddr >> 8) & 7);
		const int y3to5 = ((bankAddr >> 5) & 7) << 3;
		const int y6to7 = ((bankAddr >> 11) & 3) << 6;
		const int yDestPos = y0to2 | y3to5 | y6to7;	// or offsets together

		// determine dest pointer for scanline
		uint32_t* pLineAddr = pGraphicsView->GetPixelBuffer() + (yDestPos * kGraphicsViewerWidth);

		// pixel line
		for (int x = 0; x < 256 / 8; x++)
		{
			const uint8_t charLine = pBank->Memory[bankAddr];
			const FCodeAnalysisPage& page = pBank->Pages[bankAddr >> 10];
			const uint8_t col = GetHeatmapColourForMemoryAddress(page, bankAddr, state.CurrentFrameNo, viewerState.HeatmapThreshold);

			for (int xpix = 0; xpix < 8; xpix++)
			{
				const bool bSet = (charLine & (1 << (7 - xpix))) != 0;
				const uint32_t colRGBA = bSet ? g_kColourLUT[col] : 0xff000000;
				*(pLineAddr + xpix + (x * 8)) = colRGBA;
			}

			bankAddr++;
		}
	}

	pGraphicsView->Draw();
}
