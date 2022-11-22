#include <imgui.h>
#include "GuiUnitTest.h"

GuiUnitTest::GuiUnitTest()
{
	m_CurrStep = m_Steps.begin();
}

void GuiUnitTest::LoadStepsFromScriptFile(std::string script_file_name)
{
	int i = 0;
}

void GuiUnitTest::ProcessStep()
{

}

void GuiUnitTest::Step()
{
	if (m_CurrStep == m_Steps.end())
		return;

	if (m_CurrStep->step_type == StepType::sleep && !m_CurrStep->WaitTimeExpired())
		return;

	std::advance(m_CurrStep, 1);
	m_CurrStep->start_time = std::chrono::steady_clock::now();
	ProcessStep();
}

