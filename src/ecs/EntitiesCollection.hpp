#pragma once
#include "Entity.hpp"
#include "EntityHandle.hpp"
#include "ecs/ComponentHandle.hpp"
#include "ecs/EntityHierarchyManager.hpp"
#include "ecs/ECSApiDef.hpp"
#include "storage/MemoryPool.hpp"

#include <typeindex>

namespace ecs
{

class Manager;

class EntitiesCollection
{
	friend struct Entity;
	friend struct EntityHandle;

public:
	EntitiesCollection() = delete;
	explicit ECS_API EntitiesCollection(Manager& ecsManager);

	// Disable collection copy
	EntitiesCollection(const EntitiesCollection&) = delete;
	EntitiesCollection& operator=(const EntitiesCollection&) = delete;

	ECS_API Entity* GetEntity(const EntityHandle& handle);
	EntityHandle ECS_API GetEntityHandleById(const EntityId id);
	EntityHandle ECS_API CreateEntity();

	void ECS_API AddChild(Entity& entity, Entity& child);
	void ECS_API RemoveChild(Entity& entity, Entity& child);
	void ECS_API ClearChildren(Entity& entity, bool destroyChildren = false);
	EntityId ECS_API GetChildByIdx(Entity& entity, const std::size_t idx) const;

	void ECS_API ActivateEntity(Entity& entity, const bool activate);

	EntityHandle ECS_API CloneEntity(const EntityHandle& handle);

	// Entity hierarchy manager interface copy
	bool ECS_API CompareEntitiesInHierarchy(const Entity& lhs, const Entity& rhs) const;
	std::size_t ECS_API GetEntitiesCountInBranch(const EntityId& rootEntityId) const;
	std::size_t ECS_API GetActiveEntitiesCountInBranch(const EntityId& rootEntityId) const;
	int ECS_API GetEntityHierarchyOffsetRelativeToEntity(const EntityId& entityId, const EntityId& pivotId) const;

public:
	struct EntityHierarchyData
	{
		uint32_t nextItemPtr = Entity::GetInvalidId();
		uint32_t childId = Entity::GetInvalidId();
	};
	using EntityHierarchyDataStorageType = detail::MemoryPool<EntityHierarchyData>;
	using EntityHandlesStorageType = detail::MemoryPool<EntityHandle::HandleIndex>;

	// Helper structure to make the particular entity children iterator
	struct ChildrenData
	{
		struct iterator
		{
			using iterator_category = std::forward_iterator_tag;
			using pointer = Entity*;
			using reference = Entity&;

			iterator() = default;
			iterator(ChildrenData& data, std::size_t offset)
				: data(data)
				, offset(offset)
			{}

			reference operator*()
			{
				EntityHandle handle = data.collection->GetEntityHandleById(data.hierarchyData[offset]->childId);
				return *data.collection->GetEntity(handle);
			}

			pointer operator->()
			{
				return &**this;
			}

			iterator& operator++()
			{
				offset = data.hierarchyData[offset]->nextItemPtr;
				return *this;
			}

			iterator operator++(int)
			{
				const auto temp(*this); ++*this; return temp;
			}

			bool operator==(const iterator& other) const
			{
				return offset == other.offset;
			}

			bool operator!=(const iterator& other) const
			{
				return !(*this == other);
			}

			ChildrenData& data;
			std::size_t offset;
		};

		ChildrenData(EntitiesCollection* collection, EntityHierarchyDataStorageType& hierarchyData
			, const std::size_t offsetBegin, const std::size_t offsetEnd)
			: offsetBegin(offsetBegin)
			, offsetEnd(offsetEnd)
			, collection(collection)
			, hierarchyData(hierarchyData)
		{}

		iterator begin()
		{
			return iterator(*this, offsetBegin);
		}

		iterator end()
		{
			return iterator(*this, offsetEnd);
		}

	private:
		EntitiesCollection* collection;
		EntityHierarchyDataStorageType& hierarchyData;
		std::size_t offsetBegin;
		std::size_t offsetEnd;
	};

	ChildrenData ECS_API GetChildrenData(Entity& entity);

public:
	struct EntityComponentMapEntry
	{
		uint32_t nextItemPtr = Entity::GetInvalidId();
		ComponentHandle handle;
	};
	using ComponentsMapStorageType = detail::MemoryPool<EntityComponentMapEntry>;

	// Helper structure to make the particular entity children iterator
	struct ComponentsData
	{
		struct iterator
		{
			using iterator_category = std::forward_iterator_tag;
			using pointer = ComponentHandle *;
			using reference = ComponentHandle&;

			iterator() = delete;
			iterator(ComponentsMapStorageType& data, std::size_t offset)
				: data(data)
				, offset(offset)
			{}

			reference operator*()
			{
				return data[offset]->handle;
			}

			pointer operator->()
			{
				return &**this;
			}

			iterator& operator++()
			{
				offset = data[offset]->nextItemPtr;
				return *this;
			}

			iterator operator++(int)
			{
				const auto temp(*this); ++*this; return temp;
			}

			bool operator==(const iterator& other) const
			{
				return offset == other.offset;
			}

			bool operator!=(const iterator& other) const
			{
				return !(*this == other);
			}

			ComponentsMapStorageType& data;
			std::size_t offset;
		};

		friend struct iterator;

		ComponentsData(ComponentsMapStorageType& storage, const std::size_t offsetBegin)
			: offsetBegin(offsetBegin)
			, storage(storage)
		{}

		iterator begin()
		{
			if (storage[offsetBegin]->handle.IsValid())
			{
				return iterator(storage, offsetBegin);
			}
			
			return end();
		}

		iterator end()
		{
			return iterator(storage, Entity::GetInvalidId());
		}

	private:
		ComponentsMapStorageType& storage;
		std::size_t offsetBegin;
	};

	ComponentsData ECS_API GetComponentsData(Entity& entity);

protected:
	using EntitiesStorageType = detail::MemoryPool<Entity>;
	ECS_API EntitiesStorageType& GetEntitiesData();

private:
	friend struct Entity;

	uint8_t ECS_API GetComponentTypeIdByTypeIndex(const std::type_index& typeIndex) const;

	void RefreshActivation(Entity& entityData, bool forceActivate = false);
	void RefreshComponentsActivation(Entity& entityData);
	void RefreshChildrenActivation(Entity& entityData);

	void RefreshHierarchyDepth(Entity& entityData, const EntityId newParentId, bool constructNewHierarchyTree);

	void OnEntityDataDestroy(const EntityId entityId);
	void DestroyEntity(const EntityId entityId);

	void OnEntityEnabled(const EntityId entityId, const bool enabled);

	void ECS_API AddComponent(Entity& entityData, const ComponentHandle& handle);
	void ECS_API RemoveComponent(Entity& entityData, const ComponentHandle& handle);
	bool ECS_API HasComponent(const Entity& entityData, const ComponentTypeId componentType) const;
	ComponentHandle ECS_API GetComponentHandle(const Entity& entityData, const ComponentTypeId componentType) const;

private:
	EntitiesStorageType m_entitiesData;
	EntityHandlesStorageType m_handles;
	ComponentsMapStorageType m_entityComponentsMapping;
	EntityHierarchyDataStorageType m_entityHierarchyData;
	EntityHierarchyManager m_hierarchyManager;
	std::size_t m_activeEntitiesCount = 0U;
	Manager& m_ecsManager;
};

} // namespace ecs
