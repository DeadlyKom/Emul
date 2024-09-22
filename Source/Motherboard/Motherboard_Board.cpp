#include "Motherboard_Board.h"
#include "Motherboard_Thread.h"
#include "AppFramework.h"

FBoard::FBoard(FName Name)
	: BoardName(Name)
{}

void FBoard::Initialize()
{
	Thread = std::make_shared<FThread>(BoardName);
	Thread->Initialize();
}

void FBoard::Shutdown()
{
	Thread->Shutdown();
}

void FBoard::Reset()
{
	Thread->Reset();
}

void FBoard::NonmaskableInterrupt()
{
	Thread->NonmaskableInterrupt();
}

void FBoard::AddDevices(const std::vector<std::shared_ptr<FDevice>>& Devices, double _Frequency)
{
	Thread->AddDevices(Devices);
	SetFrequency(_Frequency);
}

void FBoard::SetFrequency(double _Frequency)
{
	if (Frequency == _Frequency)
	{
		return;
	}

	Frequency = _Frequency;
	Thread->SetFrequency(Frequency);

	LOG_CONSOLE("[{}] : Set frequency: {}Hz", BoardName.ToString(), Frequency);
}
