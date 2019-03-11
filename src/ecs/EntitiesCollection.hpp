#pragma once
#include "Entity.hpp"
#include "ecs/ComponentHandle.hpp"
#include "storage/MemoryPool.hpp"

#include <typeindex>

namespace ecs
{

class Manager;

class EntitiesCollection
{
public:
	EntitiesCollection() = delete;
	explicit EntitiesCollection(Manager& ecsManager);

	// Disable collection copy
	EntitiesCollection(const EntitiesCollection&) = delete;
	EntitiesCollection& operator=(const EntitiesCollection&) = delete;

	Entity& GetEntity(const uint32_t id);
	Entity& CreateEntity();
	void DestroyEntity(const uint32_t id);

	void AddComponent(Entity& entity, const ComponentHandle& handle);
	void RemoveComponent(Entity& entity, const ComponentHandle& handle);
	bool HasComponent(Entity& entity, const uint8_t componentType);
	void* GetComponent(Entity& entity, const uint8_t componentType) const;

	template <typename ComponentType>
	ComponentType* GetComponent(Entity& entity) const
	{
		auto componentTypeId = GetComponentTypeIdByTypeIndex(typeid(ComponentType));
		return static_cast<ComponentType*>(GetComponent(entity, componentTypeId));
	}

	void AddChild(Entity& entity, Entity& child);
	void RemoveChild(Entity& entity, Entity& child);
	uint16_t GetChildrenCount(Entity& entity) const;
	Entity* GetParent(Entity& entity);

public:
	struct EntityHierarchyData
	{
		uint32_t nextItemPtr = Entity::k_invalidId;
		uint32_t childId = Entity::k_invalidId;
	};
	using EntityHierarchyDataStorageType = detail::MemoryPool<EntityHierarchyData>;

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
				return data.collection->GetEntity(data.hierarchyData[offset]->childId);
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

	ChildrenData GetChildrenData(Entity& entity);

protected:
	struct EntityData
	{
		Entity entity;
		uint16_t childrenCount = 0U;
		bool isAlive : 1;
	};
	using EntitiesStorageType = detail::MemoryPool<EntityData>;
	EntitiesStorageType& GetEntitiesData();

private:
	uint8_t GetComponentTypeIdByTypeIndex(const std::type_index& typeIndex) const;

private:
	struct EntityComponentMapEntry
	{
		uint32_t nextItemPtr = Entity::k_invalidId;
		ComponentHandle handle;
	};
	using ComponentsMapStorageType = detail::MemoryPool<EntityComponentMapEntry>;

	EntitiesStorageType m_entities;
	ComponentsMapStorageType m_entityComponentsMapping;
	EntityHierarchyDataStorageType m_entityHierarchyData;
	Manager& m_ecsManager;
};

} // namespace ecs
