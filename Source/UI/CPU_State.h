#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SCPU_State : public SWindow
{
public:
	SCPU_State();

	virtual void Initialize() override;
	virtual void Render() override;
};
