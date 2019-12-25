#pragma once
#include "ecs/ECSApiDef.hpp"
#include "ecs/entity/EntityData.hpp"
#include "ecs/entity/EntityComponentsCollection.hpp"

namespace ecs
{

struct EntityData;
class EntitiesCollection;
class Manager;

typedef void(*EntityComponentAddedCallback)(Entity&, const ComponentHandle&);
typedef void(*EntityComponentRemovedCallback)(Entity&, const ComponentHandle&);
typedef void(*EntityChildAddedCallback)(Entity&, EntityId);
typedef void(*EntityChildRemovedCallback)(Entity&, EntityId);
typedef void(*EntityActivatedCallback)(Entity&);
typedef void(*EntityDeactivatedCallback)(Entity&);
typedef void(*ComponentActivatedCallback)(const ComponentHandle&);
typedef void(*ComponentDeactivatedCallback)(const ComponentHandle&);

/*
* @brief Entity is a wrapper around entity data, that gives the interface for manipuling the data using ecs infrastructure.
* Entity is a kind of reference couting wrapper, each entity adds to the alive references count.
* Entity can contain no entity data, it is a valid situation, but this object cannot be used to access the data, and the
* object can be tested using simple operator bool or IsValid() method.
*
* Entity has interface to manipulate entity components
* Entity has interface to manipulate its allowed properties
* User can copy and move entity instances
* User has no access to underlying entity data
*/
struct Entity
{
	static const EntityId ECS_API GetInvalidId();

	ECS_API Entity();
	ECS_API ~Entity();

	bool ECS_API IsValid() const;
	ECS_API operator bool() const;

	void ECS_API Reset();

	Entity ECS_API Clone();

	void ECS_API AddComponent(const ComponentHandle& handle);
	void ECS_API RemoveComponent(const ComponentHandle& handle);
	bool ECS_API HasComponent(const ComponentTypeId componentType) const;
	ComponentHandle ECS_API GetComponentHandle(const ComponentTypeId componentType) const;
	EntityComponentsCollection ECS_API GetComponents() const;

	void ECS_API AddChild(Entity& child);
	void ECS_API RemoveChild(Entity& child);
	void ECS_API ClearChildren();
	ECS_API Entity& GetChildByIdx(const std::size_t idx) const;
	ECS_API EntityChildrenContainer& GetChildren() const;
	std::size_t ECS_API GetChildrenCount() const;
	Entity ECS_API GetParent() const;
	uint16_t ECS_API GetOrderInParent() const;

	EntityId ECS_API GetId() const;
	void ECS_API SetEnabled(const bool enable);
	bool ECS_API IsEnabled() const;

	template <typename ComponentType>
	bool HasComponent() const
	{
		ComponentTypeId componentTypeId = GetComponentTypeIdByIndex(typeid(ComponentType));
		return HasComponent(componentTypeId);
	}

	template <typename ComponentType>
	ComponentType* GetComponent() const
	{
		ComponentTypeId componentTypeId = GetComponentTypeIdByIndex(typeid(ComponentType));
		ComponentHandle handle = GetComponentHandle(componentTypeId);
		if (handle.IsValid())
		{
			return static_cast<ComponentType*>(DoGetComponentPtr(handle));
		}
		else
		{
			return nullptr;
		}
	}

	template <typename ComponentType>
	ComponentHandle GetComponentHandle() const
	{
		ComponentTypeId componentTypeId = GetComponentTypeIdByIndex(typeid(ComponentType));
		return GetComponentHandle(componentTypeId);
	}

	// Copy is available
	ECS_API Entity(const Entity&) noexcept;
	ECS_API Entity& operator=(const Entity&) noexcept;

	// Move is available
	ECS_API Entity(Entity&&) noexcept;
	ECS_API Entity& operator=(Entity&&) noexcept;

	bool ECS_API operator==(const Entity& other) const;
	bool ECS_API operator!=(const Entity& other) const;

	static void SetManagerInstance(Manager* manager);

private:
	ECS_API void* DoGetComponentPtr(const ComponentHandle handle) const;
	ComponentTypeId ECS_API GetComponentTypeIdByIndex(const std::type_index& index) const;

	// Must be constructed only by EntitiesCollection class
	friend class EntitiesCollection;
	friend class EntityChildrenCollection;

	Entity(EntityData* data);

	void AddRef();
	void RemoveRef();

	EntityData* GetData() const;

	static ECS_API Manager* GetManagerInstance();

	EntityData* m_data = nullptr;
};

} // namespace ecs
