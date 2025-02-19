#include "IORegisterAnalysis.h"

#include <imgui.h>
#include <vector>
#include <CodeAnalyser/CodeAnalyser.h>
#include <CodeAnalyser/UI/CodeAnalyserUI.h>

void DrawRegValueHex(uint8_t val)
{
	ImGui::Text("$%X", val);
}

void DrawRegValueDecimal(uint8_t val)
{
	ImGui::Text("%d", val);
}

int DrawRegSelectList(std::vector<FRegDisplayConfig>& regList, int selection)
{
	for (int i = 0; i < (int)regList.size(); i++)
	{
		char selectableTXT[32];
		snprintf(selectableTXT, sizeof(selectableTXT), "$%X %s", i, regList[i].Name);
		if (ImGui::Selectable(selectableTXT, selection == i))
		{
			selection = i;
		}
	}

	return selection;
}

void DrawRegDetails(FC64IORegisterInfo& reg, const FRegDisplayConfig& regConfig, FCodeAnalysisState* pCodeAnalysis)
{
	if (ImGui::Button("Clear"))
	{
		reg.LastVal = 0;
		reg.Accesses.clear();
	}
	// move out into function?
	ImGui::Text("Last Val:");
	regConfig.UIDrawFunction(reg.LastVal);
	ImGui::Text("Accesses:");
	for (auto& access : reg.Accesses)
	{
		ImGui::Separator();
		ImGui::Text("Code at: $%X", access.first);
		DrawAddressLabel(*pCodeAnalysis, pCodeAnalysis->GetFocussedViewState(), access.first);

		ImGui::Text("Values:");

		for (auto& val : access.second.WriteVals)
		{
			regConfig.UIDrawFunction(val);
		}
	}
}
