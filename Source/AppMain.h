#pragma once

#include <Core/AppFramework.h>

class FAppMain : public FAppFramework
{
public:
	FAppMain();

	// virtual functions
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void Render() override;
	virtual bool IsOver() override;

private:
	void LoadIniSettings();
	void ShowSelectionWindow();

	bool bOpen;
	int32_t ItemSelectedIndex;
};
