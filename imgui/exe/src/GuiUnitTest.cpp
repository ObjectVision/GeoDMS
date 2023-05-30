#include <imgui.h>
#include "GuiUnitTest.h"
#include <sstream>
#include <string>
#include <fstream>
#include "TicInterface.h"
#include "ser/AsString.h"

GuiUnitTest::GuiUnitTest()
{
	m_CurrStep = m_Steps.begin();
}

StepType GuiUnitTest::InterpretStepType(std::string_view sv)
{
	if (sv.compare("sleep") == 0)
		return StepType::sleep;
	else if (sv.compare("open") == 0)
		return StepType::open;
	else if (sv.compare("close") == 0)
		return StepType::close;
	else if (sv.compare("reopen") == 0)
		return StepType::reopen;
	else if (sv.compare("move") == 0)
		return StepType::move;
	else if (sv.compare("dock") == 0)
		return StepType::dock;
	else if (sv.compare("expand") == 0)
		return StepType::expand;
	else if (sv.compare("set") == 0)
		return StepType::set;
	else if (sv.compare("check") == 0)
		return StepType::check;

	return StepType::none;
}

StepSubType GuiUnitTest::InterpretStepSubType(std::string_view sv)
{
	if (sv.compare("edit_palette") == 0)
		return StepSubType::edit_palette;
	else if (sv.compare("active_window") == 0)
		return StepSubType::active_window;
	else if (sv.compare("current_item") == 0)
		return StepSubType::current_item;
	else if (sv.compare("config") == 0)
		return StepSubType::config;
	else if (sv.compare("map_view") == 0)
		return StepSubType::map_view;
	else if (sv.compare("table_view") == 0)
		return StepSubType::table_view;
	else if (sv.compare("default_view") == 0)
		return StepSubType::default_view;
	else if (sv.compare("focus_view") == 0)
		return StepSubType::focus_view;
	else if (sv.compare("treeview") == 0)
		return StepSubType::treeview;
	else if (sv.compare("toolbar") == 0)
		return StepSubType::toolbar;
	else if (sv.compare("eventlog") == 0)
		return StepSubType::eventlog;
	else if (sv.compare("detail_pages") == 0)
		return StepSubType::detail_pages;
	else if (sv.compare("current_item_bar") == 0)
		return StepSubType::current_item_bar;
	return StepSubType::none;
}

std::string StepTypeToString(StepType t)
{
	switch (t)
	{
	case StepType::sleep: return "sleep";
	case StepType::open: return "open";
	case StepType::close: return "close";
	case StepType::reopen: return "reopen";
	case StepType::move: return "move";
	case StepType::dock: return "dock";
	case StepType::expand: return "expand";
	case StepType::set: return "set";
	case StepType::check: return "check";
	case StepType::none: return "none";
	default: return "";
	}
}

std::string StepSubTypeToString(StepSubType t)
{
	switch (t)
	{
	case StepSubType::none: return "none";
	case StepSubType::edit_palette: return "edit_palette";
	case StepSubType::current_item: return "current_item";
	case StepSubType::config: return "config";
	case StepSubType::active_window: return "active_window";
	case StepSubType::map_view: return "nmap_viewone";
	case StepSubType::table_view: return "table_view";
	case StepSubType::default_view: return "default_view";
	case StepSubType::focus_view: return "focus_view";
	case StepSubType::treeview: return "treeview";
	case StepSubType::toolbar: return "toolbar";
	case StepSubType::eventlog: return "eventlog";
	case StepSubType::detail_pages: return "detail_pages";
	case StepSubType::current_item_bar: return "current_item_bar";
	default: return "";
	}
}

void GuiUnitTest::LoadStepsFromScriptFile(std::string_view script_file_name)
{
	std::ifstream infile(script_file_name.data());
	std::string line;
	while (std::getline(infile, line))
	{
		StepDescription step;
		auto line_parts = DivideTreeItemFullNameIntoTreeItemNames(line, " ");
		if (line_parts.size()>=1)
			step.step_type = InterpretStepType(line_parts.at(0));
		if (line_parts.size() >= 2 && step.step_type == StepType::sleep)
			step.wait_time = std::stoi(line_parts.at(1));
		else if (line_parts.size() >= 2)
			step.step_sub_type = InterpretStepSubType(line_parts.at(1));
		if (line_parts.size() >= 3)
			step.value = line_parts.at(2);

		if (step.step_type != StepType::none && (step.wait_time || step.step_sub_type != StepSubType::none))
			m_Steps.emplace_back(std::move(step));
	}


	m_CurrStep = m_Steps.begin();
}

int GuiUnitTest::ProcessStep(GuiState& state)
{
	auto event_queues = GuiEventQueues::getInstance();

	if (m_CurrStep == m_Steps.end())
		return 0;

	if (m_CurrStep->first_time_processed)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "Gui unit test step: %s %s %s %d", StepTypeToString(m_CurrStep->step_type), StepSubTypeToString(m_CurrStep->step_sub_type), m_CurrStep->value, m_CurrStep->wait_time);
		m_CurrStep->first_time_processed = false;
	}

	if (!m_CurrStep->WaitTimeExpired())
		return 0;

	switch (m_CurrStep->step_type)
	{
	case StepType::open:
	{
		switch (m_CurrStep->step_sub_type)
		{
		case StepSubType::config:			state.configFilenameManager.Set(m_CurrStep->value); break;
		case StepSubType::default_view:		event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow); break;
		case StepSubType::table_view:		event_queues->MainEvents.Add(GuiEvents::OpenNewTableViewWindow); break;
		case StepSubType::map_view:  		event_queues->MainEvents.Add(GuiEvents::OpenNewMapViewWindow); break;
		case StepSubType::treeview:			state.ShowTreeviewWindow    = true; break;
		case StepSubType::toolbar:			state.ShowToolbar           = true; break;
		case StepSubType::eventlog:			state.ShowEventLogWindow    = true; break;
		case StepSubType::detail_pages:		state.ShowDetailPagesWindow = true; break;
		case StepSubType::current_item_bar: state.ShowCurrentItemBar    = true; break;
		}
		break;
	}
	case StepType::set:
	{
		switch (m_CurrStep->step_sub_type)
		{
		case StepSubType::current_item:
		{
			if (state.GetRoot())
			{
				auto jumpItem = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), m_CurrStep->value.c_str());

				if (jumpItem.first)
				{
					state.SetCurrentItem(const_cast<TreeItem*>(jumpItem.first));
					event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
					event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
					event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
					event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
				}
			}
		}
		}
		break;
	}
	case StepType::check:
	{
		switch (m_CurrStep->step_sub_type)
		{
		case (StepSubType::config):
		{
			if (state.configFilenameManager._Get().compare(m_CurrStep->value)!=0)
				state.return_value = 1;
			break;
		}
		case (StepSubType::current_item):
		{
			auto check_item = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), m_CurrStep->value.c_str());
			if (!check_item.first)
				state.return_value = 1; // failed unit test 
		    else if (check_item.first != state.GetCurrentItem())
				state.return_value = 1;
			break;
		}
		case StepSubType::treeview:			state.return_value = (int)state.ShowTreeviewWindow != std::stoi(m_CurrStep->value); state.return_value = 1; break; // TODO: continue implementation
		case StepSubType::toolbar:			state.ShowToolbar = (int)state.ShowToolbar != std::stoi(m_CurrStep->value); state.return_value = 1; break;
		case StepSubType::eventlog:			state.ShowEventLogWindow = (int)state.ShowEventLogWindow != std::stoi(m_CurrStep->value); state.return_value = 1; break;
		case StepSubType::detail_pages:		state.ShowDetailPagesWindow = (int)state.ShowDetailPagesWindow != std::stoi(m_CurrStep->value); state.return_value = 1; break;
		case StepSubType::current_item_bar: state.ShowCurrentItemBar = (int)state.ShowCurrentItemBar != std::stoi(m_CurrStep->value); state.return_value = 1; break;
		}
		break;
	}
	case StepType::close:
	{
		switch (m_CurrStep->step_sub_type)
		{
		case (StepSubType::config):	return 1;
		case StepSubType::treeview:			state.ShowTreeviewWindow = false; break;
		case StepSubType::toolbar:			state.ShowToolbar = false; break;
		case StepSubType::eventlog:			state.ShowEventLogWindow = false; break;
		case StepSubType::detail_pages:		state.ShowDetailPagesWindow = false; break;
		case StepSubType::current_item_bar: state.ShowCurrentItemBar = false; break;
		}
	}
	}
	m_CurrStep->is_processed = true;
	return 0;
}

void GuiUnitTest::Step()
{
	if (m_CurrStep == m_Steps.end())
		return;

	if (!m_CurrStep->is_processed) // precondition
		return;
	
	std::advance(m_CurrStep, 1);

	if (m_CurrStep == m_Steps.end())
		return;
	
	m_CurrStep->start_time = std::chrono::steady_clock::now();
}