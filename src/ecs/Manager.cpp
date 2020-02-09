#include "Manager.hpp"
#include "ecs/component/ComponentCollectionImpl.hpp"
#include "ecs/detail/Hash.hpp"
#include <algorithm>

namespace
{
ecs::Manager* ManagerInstance = nullptr;

std::size_t ComponentTypesListHash(const std::vector<ecs::ComponentTypeId>& typeIds)
{
	if (typeIds.empty())
		return 0U;

	std::size_t outHash = std::hash<ecs::ComponentTypeId>()(typeIds[0]);
	for (std::size_t i = 1; i < typeIds.size(); ++i)
	{
		ecs::detail::hash_combine(outHash, std::hash<ecs::ComponentTypeId>()(typeIds[i]));
	}

	return outHash;
}
}

namespace ecs
{

const std::string k_invalidComponentName = "[UNDEFINED]";

Manager::Manager()
{}

System* Manager::GetSystemByTypeIndex(const std::type_index& typeIndex) const
{
	auto it = m_systemsTypeIdMapping.find(typeIndex);
	if (it != m_systemsTypeIdMapping.end())
	{
		return it->second;
	}

	return nullptr;
}

void Manager::AddSystemToStorage(SystemPtr&& system)
{
	m_systemsStorage.push_back(std::move(system));
	AddSystem(m_systemsStorage.back().get());
}

void Manager::AddSystem(System* system)
{
	m_systemsTypeIdMapping.emplace(typeid(*system), system);
	m_newSystems.emplace_back(system, m_isUpdatingSystems);

	if (!m_isUpdatingSystems)
	{
		AddSystemToOrderedSystemsList(system);
	}
}

void Manager::RemoveSystem(System* system)
{
	if (m_isUpdatingSystems)
	{
		m_removedSystems.push_back(system);
	}
	else
	{
		DoRemoveSystem(system);
	}
}

void Manager::DoRemoveSystem(System* system)
{
	system->Destroy();

	{
		auto it = m_systemsTypeIdMapping.find(typeid(*system));
		if (it != m_systemsTypeIdMapping.end())
		{
			m_systemsTypeIdMapping.erase(it);
		}
	}
	
	{
		auto it = std::find(m_orderedSystems.begin(), m_orderedSystems.end(),system);
		if (it != m_orderedSystems.end())
		{
			m_orderedSystems.erase(it);
		}
	}

	{
		auto predicate = [system](const SystemPtr& systemPtr)
		{
			return systemPtr.get() == system;
		};

		auto it = std::find_if(m_systemsStorage.begin(), m_systemsStorage.end(), predicate);
		if (it != m_systemsStorage.end())
		{
			m_systemsStorage.erase(it);
		}
	}
}

void Manager::Init()
{
	
}

void Manager::Destroy()
{
	m_isBeingDestroyed = true;

	// Destroy systems
	for (ecs::System* system : m_orderedSystems)
	{
		system->Destroy();
	}
	m_systemsTypeIdMapping.clear();
	m_systemsStorage.clear();
	m_orderedSystems.clear();

	// Destroy entities
	m_entitiesCollection.Clear();

	// Destroy components
	for (auto& storage : m_componentStorages)
	{
		storage->Clear();
		storage.reset();
	}
	m_componentStorages.clear();

	m_componentTypeIndexes.clear();
	m_componentNameToIdMapping.clear();
	m_typeIndexToComponentTypeIdMapping.clear();

	m_isBeingDestroyed = false;
}

void Manager::AddSystemToOrderedSystemsList(System* system)
{
	assert(!m_isUpdatingSystems);

	auto predicate = [](System* lhs, System* rhs)
	{
		return *lhs < *rhs;
	};
	auto insertIterator = std::upper_bound(m_orderedSystems.begin(), m_orderedSystems.end(), system, predicate);

	m_orderedSystems.insert(insertIterator, system);
}

void Manager::Update()
{
	if (!m_newSystems.empty())
	{
		for (auto& systemData : m_newSystems)
		{
			ecs::System* system = systemData.first;
			const bool mustBeAddedToOrderedSystemsList = systemData.second;

			if (mustBeAddedToOrderedSystemsList)
			{
				AddSystemToOrderedSystemsList(system);
			}

			system->Init();
		}

		m_newSystems.clear();
	}

	if (m_systemPrioritiesChanged)
	{
		SortOrderedSystemsList();
	}

	UpdateSystems();

	// Remove system that are waiting for removal
	if (!m_removedSystems.empty())
	{
		for (ecs::System* system : m_removedSystems)
		{
			DoRemoveSystem(system);
		}

		m_removedSystems.clear();
	}
}

void Manager::UpdateSystems()
{
	m_isUpdatingSystems = true;

	for (ecs::System* system : m_orderedSystems)
	{
		system->Update();
	}

	m_isUpdatingSystems = false;
}

void Manager::SortOrderedSystemsList()
{
	auto predicate = [](System* lhs, System* rhs)
	{
		return *lhs < *rhs;
	};
	std::stable_sort(m_orderedSystems.begin(), m_orderedSystems.end(), predicate);

	m_systemPrioritiesChanged = false;
}

ComponentPtr Manager::CreateComponentByName(const std::string& name)
{
	auto typeIdIt = m_componentNameToIdMapping.find(name);
	if (typeIdIt != m_componentNameToIdMapping.end())
	{
		return CreateComponentInternal(typeIdIt->second);
	}

	return ComponentPtr();
}

ComponentPtr Manager::CreateComponentByTypeId(const ComponentTypeId typeId)
{
	return CreateComponentInternal(typeId);
}

ComponentPtr Manager::CreateComponentInternal(const ComponentTypeId typeId)
{
	return GetCollection(typeId)->Create();
}

void Manager::ReleaseComponent(ComponentTypeId componentType, int32_t index)
{
	GetCollection(componentType)->Destroy(index);
}

ComponentTypeId Manager::GetComponentTypeIdByIndex(const std::type_index& typeIndex) const
{
	auto it = m_typeIndexToComponentTypeIdMapping.find(typeIndex);
	if (it != m_typeIndexToComponentTypeIdMapping.end())
	{
		return it->second;
	}

	return GetInvalidComponentTypeId();
}

ComponentTypeId Manager::GetComponentTypeIdByName(const std::string& name) const
{
	auto it = m_componentNameToIdMapping.find(name);
	if (it != m_componentNameToIdMapping.end())
	{
		return it->second;
	}

	return GetInvalidComponentTypeId();
}

std::type_index Manager::GetComponentTypeIndexByTypeId(const ComponentTypeId typeId) const
{
	return m_componentTypeIndexes[typeId];
}

IComponentCollection* Manager::GetCollection(const ComponentTypeId typeId) const
{
	assert(typeId < m_componentStorages.size());
	return m_componentStorages[typeId].get();
}

EntitiesCollection& Manager::GetEntitiesCollection()
{
	return m_entitiesCollection;
}

void Manager::RegisterComponentTypeInternal(const std::string& name, const std::type_index& typeIndex, const ComponentTypeId typeId, std::unique_ptr<IComponentCollection>&& collection)
{
	// Register type storage
	m_componentStorages.emplace_back(std::move(collection));
	m_componentNameToIdMapping.emplace(name, typeId);
	m_componentTypeIndexes.push_back(typeIndex);
	m_typeIndexToComponentTypeIdMapping.emplace(typeIndex, typeId);
}

ComponentsTupleCache* Manager::GetComponentsTupleCache(const std::vector<ComponentTypeId>& typeIds)
{
	uint32_t hash = static_cast<uint32_t>(ComponentTypesListHash(typeIds));
	auto it = m_tupleCaches.find(hash);
	if (it != m_tupleCaches.end())
	{
		return &it->second;
	}

	return nullptr;
}

GenericComponentsCacheView Manager::GetComponentsTuple(const std::vector<ComponentTypeId>& typeIds)
{
	ComponentsTupleCache* tupleCache = GetComponentsTupleCache(typeIds);
	return GenericComponentsCacheView(tupleCache);
}

const std::string& Manager::GetComponentNameByTypeId(const ComponentTypeId typeId) const
{
	auto predicate = [typeId](const std::pair<std::string, ComponentTypeId>& data)
	{
		return typeId == data.second;
	};
	auto it = std::find_if(m_componentNameToIdMapping.begin(), m_componentNameToIdMapping.end(), predicate);

	if (it != m_componentNameToIdMapping.end())
	{
		return it->first;
	}

	return k_invalidComponentName;
}

const std::string& Manager::GetComponentNameByTypeId(const std::type_index& typeIndex) const
{
	auto it = m_typeIndexToComponentTypeIdMapping.find(typeIndex);
	if (it != m_typeIndexToComponentTypeIdMapping.end())
	{
		return GetComponentNameByTypeId(it->second);
	}
	
	return k_invalidComponentName;
}

ComponentPtr Manager::CloneComponent(const ComponentPtr& handle)
{
	auto collection = GetCollection(handle.GetTypeId());
	return collection->CloneComponent(handle.m_block->dataIndex);
}

void Manager::MoveComponentData(const ComponentPtr& handle, void* dataPtr)
{
	IComponentCollection* collection = GetCollection(handle.GetTypeId());
	collection->MoveData(handle.m_block->dataIndex, dataPtr);
}

void Manager::NotifySystemPriorityChanged()
{
	m_systemPrioritiesChanged = true;
}

ComponentCreateDelegate& Manager::GetComponentCreateDelegate()
{
	return m_componentCreateDelegate;
}

ComponentDestroyDelegate& Manager::GetComponentDestroyDelegate()
{
	return m_componentDestroyDelegate;
}

ComponentAttachedDelegate& Manager::GetComponentAttachedDelegate()
{
	return m_componentAttachedDelegate;
}

ComponentDetachedDelegate& Manager::GetComponentDetachedDelegate()
{
	return m_componentDetachedDelegate;
}

EntityCreateDelegate& Manager::GetEntityCreateDelegate()
{
	return m_entityCreateDelegate;
}

EntityDestroyDelegate& Manager::GetEntityDestroyDelegate()
{
	return m_entityDestroyDelegate;
}

Entity Manager::GetEntityById(const EntityId id)
{
	return m_entitiesCollection.GetEntityById(id);
}

Entity Manager::CreateEntity()
{
	return m_entitiesCollection.CreateEntity();
}

void* Manager::GetComponentRaw(ComponentTypeId componentType, int32_t index)
{
	auto collection = GetCollection(componentType);
	if (nullptr != collection)
	{
		return collection->GetData(index);
	}

	return nullptr;
}

Manager* Manager::Get()
{
	return ManagerInstance;
}

void Manager::InitECSManager()
{
	ManagerInstance = new Manager();
}

void Manager::ShutdownECSManager()
{
	if (nullptr != ManagerInstance)
	{
		ManagerInstance->Destroy();
		delete ManagerInstance;
		ManagerInstance = nullptr;
	}
}

ComponentTypeId Manager::GetInvalidComponentTypeId()
{
	return std::numeric_limits<ComponentTypeId>::max();
}

void Manager::RegisterComponentsTupleIterator(std::vector<ComponentTypeId>& typeIds)
{
	if (typeIds.size() > 0)
	{
		uint32_t typeIdsHash = static_cast<uint32_t>(ComponentTypesListHash(typeIds));
		m_tupleCaches.emplace(std::piecewise_construct, std::forward_as_tuple(typeIdsHash), std::forward_as_tuple(typeIds.data(), typeIds.size()));
	}
}

} // namespace ecs
