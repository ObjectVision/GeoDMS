#pragma once
#include "GuiBase.h"

enum class StepType
{
	sleep,
	open,
	close,
	reopen,
	move,
	dock,
	expand,
	set,
	
	check,
};

enum class StepSubType
{
	edit_palette,
	active_window,
	new_current_item,
	new_config,
	map_view,
	table_view,
	default_view,
	focus_view,
	treeview,
	toolbar,
	eventlog,
	detail_pages,
	current_item_bar,
};

struct StepDescription
{
	StepType	step_type;
	StepSubType step_sub_type;
	std::string value;
	UInt32 wait_time = 0;
	std::chrono::steady_clock::time_point start_time;

	bool WaitTimeExpired()
	{
		if ((start_time - std::chrono::steady_clock::now()).count() > wait_time)
			return true;
		return false;
	}
};

class GuiUnitTest : GuiBaseComponent
{
public:
	GuiUnitTest();
	void Step();
	void LoadStepsFromScriptFile(std::string script_file_name);

private:
	void ProcessStep();
	std::list<StepDescription>	         m_Steps;
	std::list<StepDescription>::iterator m_CurrStep;
	GuiState			                 m_State;
};