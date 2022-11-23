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
	none
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
	none
};

struct StepDescription
{
	StepType	step_type	  = StepType::none;
	StepSubType step_sub_type = StepSubType::none;
	std::string value;
	UInt64 wait_time = 0;
	bool is_processed = false;
	std::chrono::steady_clock::time_point start_time;

	bool WaitTimeExpired()
	{
		UInt64 count = (std::chrono::steady_clock::now() - start_time).count() / 1000000000;
		if (count > wait_time)
			return true;
		return false;
	}
};

class GuiUnitTest : GuiBaseComponent
{
public:
	GuiUnitTest();
	void ProcessStep();
	void Step();
	void LoadStepsFromScriptFile(std::string_view script_file_name);

private:
	StepType InterpretStepType(std::string_view sv);
	StepSubType InterpretStepSubType(std::string_view sv);
	std::list<StepDescription>	         m_Steps;
	std::list<StepDescription>::iterator m_CurrStep;
	GuiState			                 m_State;
};