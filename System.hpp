#pragma once

namespace ecs
{

// Dummy class, which should be implemented
class System
	: public ILifecycleCallback
{
public:
	virtual ~System() = default;
};

} // namespace ecs
