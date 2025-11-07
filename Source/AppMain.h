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
	virtual void LoadSettings() override;
	virtual std::string_view GetDefaultConfig() override;

private:
	void ShowSelectionWindow();

	bool bOpen;
	bool bRememberChoice;
	int32_t ItemSelectedIndex;
};
