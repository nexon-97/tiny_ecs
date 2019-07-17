#pragma once
#include "ecs/ECSApiDef.hpp"
#include "ecs/entity/EntityData.hpp"
#include "ecs/entity/Entity.hpp"
#include "ecs/component/ComponentHandle.hpp"
#include "ecs/storage/MemoryPool.hpp"

#include <unordered_map>

namespace ecs
{

class Manager;

class EntitiesCollection
{
	friend struct Entity;

	using EntityIdsMap = std::unordered_map<EntityId, uint32_t>; // Mapping of entity id to entity storage location

public:
	EntitiesCollection() = delete;
	explicit ECS_API EntitiesCollection(Manager& ecsManager);

	// Disable collection copy
	EntitiesCollection(const EntitiesCollection&) = delete;
	EntitiesCollection& operator=(const EntitiesCollection&) = delete;

	Entity ECS_API GetEntityById(const EntityId id);
	Entity ECS_API CreateEntity();

	void ECS_API ActivateEntity(Entity& entity, const bool activate);
	bool ECS_API IsEntityActivated(Entity& entity) const;

private:
	using EntitiesStorageType = detail::MemoryPool<EntityData>;
	ECS_API EntitiesStorageType& GetEntitiesData();

	void RefreshActivation(EntityData& entityData, bool forceActivate = false);
	void RefreshComponentsActivation(EntityData& entityData);
	void RefreshChildrenActivation(EntityData& entityData);

	void OnEntityDataDestroy(const EntityId entityId);
	void DestroyEntity(const EntityId entityId);

	void OnEntityEnabled(const EntityId entityId, const bool enabled);

	void MoveEntityData(EntityData& entityData, const uint32_t newLocation);
	Entity CloneEntity(const Entity& entity);

	EntityComponentMapEntry& CreateComponentMappingEntry(EntityData& entityData);
	EntityComponentMapEntry* FindComponentMappingEntry(EntityData& entityData, const ComponentTypeId componentType);
	void RemoveComponentMappingEntry(EntityData& entityData, const ComponentTypeId componentType);
	ComponentsMapStorageType& GetComponentsMapStorage();

private:
	EntitiesStorageType m_entitiesData;
	EntityIdsMap m_entityIdsMap;
	ComponentsMapStorageType m_entityComponentsMapping;
	std::size_t m_activeEntitiesCount = 0U;
	EntityId m_nextEntityId = 0U;
	Manager& m_manager;
};

} // namespace ecs
