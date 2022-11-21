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

void GuiUnitTest::Step()
{
	if (m_CurrStep == m_Steps.end())
		return;

	std::advance(m_CurrStep, 1);
	
	
}

