#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SDisassembler : public SWindow
{
public:
	SDisassembler();

	virtual void Initialize() override;
	virtual void Render() override;
};
#pragma once
