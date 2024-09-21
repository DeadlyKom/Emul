#pragma once

#include <CoreMinimal.h>

class FDevice : public std::enable_shared_from_this<FDevice>
{
	friend class FMotherboard;
public:
	FDevice(FName Name);

	// virtual pure methods
	virtual FName GetName() = 0;

protected:
	FName DeviceName;

private:
	void Register();
	void Unregister();

	bool bRegistered;
};
