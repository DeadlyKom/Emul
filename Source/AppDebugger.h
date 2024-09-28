#pragma once

#include <Core/AppFramework.h>

class SViewer;
class SCallStack;
class SCPU_State;
class SMemoryDump;
class SDisassembler;
class FMotherboard;

class FAppDebugger : public FAppFramework
{
	friend SViewer;
	friend SCallStack;
	friend SCPU_State;
	friend SMemoryDump;
	friend SDisassembler;

public:
	FAppDebugger();

	// virtual functions
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;
	virtual bool IsOver() override;

private:
	std::shared_ptr<SViewer> Viewer;
	std::shared_ptr<FMotherboard> Motherboard;
};
