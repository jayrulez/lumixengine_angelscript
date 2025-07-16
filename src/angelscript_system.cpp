#include "angelscript_system.h"
#include "angelscript_wrapper.h"
#include "as_script.h"
#include "core/allocator.h"
#include "core/array.h"
#include "core/associative_array.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/os.h"
#include "core/profiler.h"
#include "core/stream.h"
#include "core/string.h"
#include "engine/engine.h"
#include "engine/input_system.h"
#include "engine/plugin.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/world.h"
#include <angelscript.h>

namespace Lumix
{

static const ComponentType ANGELSCRIPT_TYPE = reflection::getComponentType("angelscript");
static const ComponentType ANGELSCRIPT_INLINE_TYPE = reflection::getComponentType("angelscript_inline");

enum class AngelScriptModuleVersion : i32
{
	HASH64,
	INLINE_SCRIPT,

	LATEST
};

struct ASScriptManager final : ResourceManager
{
	ASScriptManager(IAllocator& allocator)
		: ResourceManager(allocator)
		, m_allocator(allocator)
	{
	}

	Resource* createResource(const Path& path) override
	{
		return LUMIX_NEW(m_allocator, ASScript)(path, *this, m_allocator);
	}

	void destroyResource(Resource& resource) override { LUMIX_DELETE(m_allocator, static_cast<ASScript*>(&resource)); }

	IAllocator& m_allocator;
};

void messageCallback(const asSMessageInfo* msg, void* param)
{
	const char* type = "Error";
	if (msg->type == asMSGTYPE_WARNING)
		type = "Warning";
	else if (msg->type == asMSGTYPE_INFORMATION)
		type = "Info";

	logError("AngelScript ", type, " (", msg->row, ", ", msg->col, "): ", msg->message);
}

struct AngelScriptSystemImpl final : AngelScriptSystem
{
	explicit AngelScriptSystemImpl(Engine& engine);
	virtual ~AngelScriptSystemImpl();

	void createModules(World& world) override;
	const char* getName() const override { return "angelscript"; }
	ASScriptManager& getScriptManager() { return m_script_manager; }
	void serialize(OutputMemoryStream& stream) const override {}
	bool deserialize(i32 version, InputMemoryStream& stream) override { return version == 0; }
	asIScriptEngine* getEngine() override { return m_engine; }

	void update(float dt) override
	{
		// Update any timers, contexts, etc.
	}

	void unloadASResource(ASResourceHandle resource) override
	{
		auto iter = m_as_resources.find(resource);
		if (!iter.isValid()) return;
		Resource* res = iter.value();
		m_as_resources.erase(iter);
		res->decRefCount();
	}

	ASResourceHandle addASResource(const Path& path, ResourceType type) override
	{
		Resource* res = m_engine_ref.getResourceManager().load(type, path);
		if (!res) return 0xffFFffFF;
		++m_last_as_resource_idx;
		ASSERT(m_last_as_resource_idx != 0xffFFffFF);
		m_as_resources.insert(m_last_as_resource_idx, res);
		return m_last_as_resource_idx;
	}

	Resource* getASResource(ASResourceHandle resource) const override
	{
		auto iter = m_as_resources.find(resource);
		if (iter.isValid()) return iter.value();
		return nullptr;
	}

	TagAllocator m_allocator;
	asIScriptEngine* m_engine;
	Engine& m_engine_ref;
	ASScriptManager m_script_manager;
	AngelScriptWrapper::StringFactory m_string_factory;
	HashMap<int, Resource*> m_as_resources;
	u32 m_last_as_resource_idx = 0;
};

struct AngelScriptModuleImpl final : AngelScriptModule
{
	struct ScriptComponent;

	struct ScriptEnvironment
	{
		asIScriptModule* m_script_module = nullptr;
		asIScriptContext* m_script_context = nullptr;
	};

	struct ScriptInstance : ScriptEnvironment
	{
		enum Flags : u32
		{
			NONE = 0,
			ENABLED = 1 << 0,
			LOADED = 1 << 1,
			MOVED_FROM = 1 << 2
		};

		explicit ScriptInstance(ScriptComponent& cmp, IAllocator& allocator)
			: m_properties(allocator)
			, m_cmp(&cmp)
		{
			AngelScriptModuleImpl& module = cmp.m_module;
			asIScriptEngine* engine = module.m_system.m_engine;

			// Create script module for this instance
			static int module_counter = 0;
			m_module_name = StaticString<64>("ScriptInstance", module_counter++);
			m_script_module = engine->GetModule(m_module_name, asGM_CREATE_IF_NOT_EXISTS);

			// Create context for execution
			m_script_context = engine->CreateContext();

			m_flags = Flags(m_flags | ENABLED);
		}

		ScriptInstance(const ScriptInstance&) = delete;

		ScriptInstance(ScriptInstance&& rhs) noexcept
			: m_properties(rhs.m_properties.move())
			, m_cmp(rhs.m_cmp)
			, m_script(rhs.m_script)
			, m_flags(rhs.m_flags) 
		{
			m_script_module = rhs.m_script_module;
			m_script_context = rhs.m_script_context;
			rhs.m_script = nullptr;
			rhs.m_script_module = nullptr;
			rhs.m_script_context = nullptr;
			rhs.m_flags = Flags(rhs.m_flags | MOVED_FROM);
		}

		void operator=(ScriptInstance&& rhs) noexcept
		{
			m_properties = rhs.m_properties.move();
			m_script_module = rhs.m_script_module;
			m_script_context = rhs.m_script_context;
			m_cmp = rhs.m_cmp;
			m_script = rhs.m_script;
			m_flags = rhs.m_flags;
			rhs.m_script = nullptr;
			rhs.m_script_module = nullptr;
			rhs.m_script_context = nullptr;
			rhs.m_flags = Flags(rhs.m_flags | MOVED_FROM);
		}

		~ScriptInstance()
		{
			if (!(m_flags & MOVED_FROM))
			{
				if (m_script)
				{
					m_script->getObserverCb().unbind<&ScriptComponent::onScriptLoaded>(m_cmp);
					m_script->decRefCount();
				}

				if (m_script_context)
				{
					m_script_context->Release();
				}

				if (m_script_module)
				{
					m_script_module->Discard();
				}
			}
		}

		void onScriptUnloaded(AngelScriptModuleImpl& module,
			struct ScriptComponent& cmp,
			int scr_index)
		{
			if (m_script_module)
			{
				m_script_module->Discard();
				m_script_module = nullptr;
			}

			// Cleanup when script is unloaded
			m_flags = Flags(m_flags & ~LOADED);
		}

		void onScriptLoaded(AngelScriptModuleImpl& module,
			struct ScriptComponent& cmp,
			int scr_index)
		{
			if (/*!m_script_module || */ !m_script) return;

			if (m_script_module)
			{
				m_script_module->Discard();
			}

			asIScriptEngine* engine = module.m_system.m_engine;

			m_script_module = engine->GetModule(m_module_name, asGM_CREATE_IF_NOT_EXISTS);

			bool is_reload = m_flags & LOADED;

			// Add script section and build
			StringView source = m_script->getSourceCode();
			int r = m_script_module->AddScriptSection(m_script->getPath().c_str(), source.begin, source.size());
			if (r < 0)
			{
				logError("Failed to add script section for ", m_script->getPath());
				return;
			}

			r = m_script_module->Build();
			if (r < 0)
			{
				logError("Failed to build script ", m_script->getPath());
				return;
			}

			m_flags = Flags(m_flags | LOADED);

			// Call awake function if it exists
			asIScriptFunction* awake_func = m_script_module->GetFunctionByName("awake");
			if (awake_func && m_script_context)
			{
				m_script_context->Prepare(awake_func);
				m_script_context->Execute();
			}
		}

		ScriptComponent* m_cmp;
		ASScript* m_script = nullptr;
		StaticString<64> m_module_name;
		Array<Property> m_properties;
		Flags m_flags = Flags::NONE;
	};

	struct InlineScriptComponent : ScriptEnvironment
	{
		InlineScriptComponent(EntityRef entity, AngelScriptModuleImpl& module, IAllocator& allocator)
			: m_source(allocator)
			, m_entity(entity)
			, m_module(module)
		{
			asIScriptEngine* engine = module.m_system.m_engine;

			// Create script module for this instance
			static int module_counter = 0;
			StaticString<64> module_name("InlineScript", module_counter++);
			m_script_module = engine->GetModule(module_name, asGM_CREATE_IF_NOT_EXISTS);

			// Create context for execution
			m_script_context = engine->CreateContext();
		}

		InlineScriptComponent(InlineScriptComponent&& rhs) noexcept
			: m_module(rhs.m_module)
			, m_source(rhs.m_source)
			, m_entity(rhs.m_entity)
		{
			m_script_module = rhs.m_script_module;
			m_script_context = rhs.m_script_context;
			rhs.m_script_module = nullptr;
			rhs.m_script_context = nullptr;
		}

		void operator=(InlineScriptComponent&& rhs) = delete;

		~InlineScriptComponent()
		{
			if (m_script_context)
			{
				m_script_context->Release();
			}

			if (m_script_module)
			{
				m_script_module->Discard();
			}
		}

		void compileAndRun()
		{
			if (!m_script_module) return;

			// Add script section and build
			int r = m_script_module->AddScriptSection("main", m_source.c_str());
			if (r < 0)
			{
				logError("Failed to add script section");
				return;
			}

			r = m_script_module->Build();
			if (r < 0)
			{
				logError("Failed to build script");
				return;
			}

			// Find and execute main function
			asIScriptFunction* func = m_script_module->GetFunctionByDecl("void main()");
			if (func)
			{
				m_script_context->Prepare(func);
				m_script_context->Execute();
			}
		}

		AngelScriptModuleImpl& m_module;
		EntityRef m_entity;
		String m_source;
	};

	struct ScriptComponent
	{
		ScriptComponent(AngelScriptModuleImpl& module, EntityRef entity, IAllocator& allocator)
			: m_scripts(allocator)
			, m_module(module)
			, m_entity(entity)
		{
		}

		void onScriptLoaded(Resource::State old_state, Resource::State new_state, Resource& resource)
		{
			for (int scr_index = 0, c = m_scripts.size(); scr_index < c; ++scr_index)
			{
				ScriptInstance& script = m_scripts[scr_index];

				if (!script.m_script) continue;
				if (script.m_script != &resource) continue;
				if (new_state == Resource::State::READY)
				{
					script.onScriptLoaded(m_module, *this, scr_index);
				}
				else if (new_state == Resource::State::EMPTY)
				{
					script.onScriptUnloaded(m_module, *this, scr_index);
				}
			}
		}

		Array<ScriptInstance> m_scripts;
		AngelScriptModuleImpl& m_module;
		EntityRef m_entity;
	};

	struct FunctionCall : IFunctionCall
	{
		void add(int parameter) override
		{
			// TODO: Add parameter to context
			++parameter_count;
		}

		void add(EntityPtr parameter) override
		{
			// TODO: Add parameter to context
			++parameter_count;
		}

		void add(bool parameter) override
		{
			// TODO: Add parameter to context
			++parameter_count;
		}

		void add(float parameter) override
		{
			// TODO: Add parameter to context
			++parameter_count;
		}

		void add(void* parameter) override
		{
			// TODO: Add parameter to context
			++parameter_count;
		}

		void addEnvironment(asIScriptModule* module) override
		{
			this->module = module;
			++parameter_count;
		}

		World* world = nullptr;
		int parameter_count = 0;
		asIScriptModule* module = nullptr;
		asIScriptContext* context = nullptr;
		bool is_in_progress = false;
	};

public:
	AngelScriptModuleImpl(AngelScriptSystemImpl& system, World& world)
		: m_system(system)
		, m_world(world)
		, m_scripts(system.m_allocator)
		, m_inline_scripts(system.m_allocator)
		, m_property_names(system.m_allocator)
		, m_is_game_running(false)
	{
		m_function_call.is_in_progress = false;
	}

	int getVersion() const override { return (int)AngelScriptModuleVersion::LATEST; }
	const char* getName() const override { return "angelscript"; }

	IFunctionCall* beginFunctionCall(const ScriptEnvironment& env, const char* function)
	{
		if (!env.m_script_module || !env.m_script_context) return nullptr;

		asIScriptFunction* func = env.m_script_module->GetFunctionByName(function);
		if (!func) return nullptr;

		m_function_call.context = env.m_script_context;
		m_function_call.module = env.m_script_module;
		m_function_call.world = &m_world;
		m_function_call.is_in_progress = true;
		m_function_call.parameter_count = 0;

		return &m_function_call;
	}

	IFunctionCall* beginFunctionCallInlineScript(EntityRef entity, const char* function) override
	{
		ASSERT(!m_function_call.is_in_progress);
		auto iter = m_inline_scripts.find(entity);
		if (!iter.isValid()) return nullptr;

		InlineScriptComponent& script = iter.value();
		return beginFunctionCall(script, function);
	}

	IFunctionCall* beginFunctionCall(EntityRef entity, int scr_index, const char* function) override
	{
		ASSERT(!m_function_call.is_in_progress);
		auto iter = m_scripts.find(entity);
		if (!iter.isValid()) return nullptr;

		ScriptComponent* script_cmp = iter.value();
		auto& script = script_cmp->m_scripts[scr_index];
		return beginFunctionCall(script, function);
	}

	void endFunctionCall() override
	{
		ASSERT(m_function_call.is_in_progress);
		m_function_call.is_in_progress = false;
		// TODO: Execute the function call with parameters
	}

	int getPropertyCount(EntityRef entity, int scr_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_properties.size();
	}

	const char* getPropertyName(EntityRef entity, int scr_index, int prop_index) override
	{
		return getPropertyName(m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].name_hash);
	}

	ResourceType getPropertyResourceType(EntityRef entity, int scr_index, int prop_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].resource_type;
	}

	Property::Type getPropertyType(EntityRef entity, int scr_index, int prop_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].type;
	}

	~AngelScriptModuleImpl()
	{
		for (auto* script_cmp : m_scripts)
		{
			ASSERT(script_cmp);
			LUMIX_DELETE(m_system.m_allocator, script_cmp);
		}
	}

	bool execute(EntityRef entity, i32 scr_index, StringView code) override
	{
		const ScriptInstance& script = m_scripts[entity]->m_scripts[scr_index];

		if (!script.m_script_module || !script.m_script_context) return false;

		// Add code as script section and build
		int r = script.m_script_module->AddScriptSection("temp", code.begin, code.size());
		if (r < 0) return false;

		r = script.m_script_module->Build();
		if (r < 0) return false;

		// Execute main function if it exists
		asIScriptFunction* func = script.m_script_module->GetFunctionByDecl("void main()");
		if (func)
		{
			script.m_script_context->Prepare(func);
			r = script.m_script_context->Execute();
			return r == asEXECUTION_FINISHED;
		}

		return true;
	}

	asIScriptContext* getContext(EntityRef entity, int scr_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_script_context;
	}

	asIScriptModule* getScriptModule(EntityRef entity, int scr_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_script_module;
	}

	World& getWorld() override { return m_world; }

	void startGame() override { m_is_game_running = true; }

	void stopGame() override { m_is_game_running = false; }

	void createInlineScriptComponent(EntityRef entity)
	{
		m_inline_scripts.insert(entity, InlineScriptComponent(entity, *this, m_system.m_allocator));
		m_world.onComponentCreated(entity, ANGELSCRIPT_INLINE_TYPE, this);
	}

	void destroyInlineScriptComponent(EntityRef entity)
	{
		m_inline_scripts.erase(entity);
		m_world.onComponentDestroyed(entity, ANGELSCRIPT_INLINE_TYPE, this);
	}

	void createScriptComponent(EntityRef entity)
	{
		auto& allocator = m_system.m_allocator;
		ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, entity, allocator);
		m_scripts.insert(entity, script);
		m_world.onComponentCreated(entity, ANGELSCRIPT_TYPE, this);
	}

	void destroyScriptComponent(EntityRef entity)
	{
		ScriptComponent* cmp = m_scripts[entity];
		LUMIX_DELETE(m_system.m_allocator, cmp);
		m_scripts.erase(entity);
		m_world.onComponentDestroyed(entity, ANGELSCRIPT_TYPE, this);
	}

	void setPropertyValue(EntityRef entity, int scr_index, const char* name, const char* value) override
	{
		auto* script_cmp = m_scripts[entity];
		if (!script_cmp) return;
		Property& prop = getScriptProperty(entity, scr_index, name);
		prop.stored_value = value;
	}

	void getPropertyValue(EntityRef entity, int scr_index, const char* property_name, Span<char> out) override
	{
		ASSERT(out.length() > 0);

		const StableHash hash(property_name);
		auto& inst = m_scripts[entity]->m_scripts[scr_index];
		for (auto& prop : inst.m_properties)
		{
			if (prop.name_hash == hash)
			{
				copyString(out, prop.stored_value);
				return;
			}
		}
		out[0] = '\0';
	}

	const char* getPropertyName(StableHash name_hash) const
	{
		auto iter = m_property_names.find(name_hash);
		if (iter.isValid()) return iter.value().c_str();
		return "N/A";
	}

	void serialize(OutputMemoryStream& serializer) override
	{
		serializer.write(m_inline_scripts.size());
		for (auto iter : m_inline_scripts.iterated())
		{
			serializer.write(iter.key());
			serializer.write(iter.value().m_source);
		}

		serializer.write(m_scripts.size());
		for (ScriptComponent* script_cmp : m_scripts)
		{
			serializer.write(script_cmp->m_entity);
			serializer.write(script_cmp->m_scripts.size());
			for (auto& scr : script_cmp->m_scripts)
			{
				serializer.writeString(scr.m_script ? scr.m_script->getPath() : Path());
				serializer.write(scr.m_flags);
				serializer.write(scr.m_properties.size());
				for (Property& prop : scr.m_properties)
				{
					serializer.write(prop.name_hash);
					serializer.write(prop.type);
					serializer.writeString(prop.stored_value);
				}
			}
		}
	}

	void deserialize(InputMemoryStream& serializer, const EntityMap& entity_map, i32 version) override
	{
		const i32 inline_len = serializer.read<i32>();
		m_inline_scripts.reserve(inline_len);
		for (int i = 0; i < inline_len; ++i)
		{
			EntityRef entity;
			serializer.read(entity);
			entity = entity_map.get(entity);
			auto iter = m_inline_scripts.insert(entity, InlineScriptComponent(entity, *this, m_system.m_allocator));
			serializer.read(iter.value().m_source);
			m_world.onComponentCreated(entity, ANGELSCRIPT_INLINE_TYPE, this);
		}

		int len = serializer.read<int>();
		m_scripts.reserve(len + m_scripts.size());
		for (int i = 0; i < len; ++i)
		{
			auto& allocator = m_system.m_allocator;
			EntityRef entity;
			serializer.read(entity);
			entity = entity_map.get(entity);
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, entity, allocator);

			m_scripts.insert(script->m_entity, script);
			int scr_count;
			serializer.read(scr_count);
			for (int scr_idx = 0; scr_idx < scr_count; ++scr_idx)
			{
				auto& scr = script->m_scripts.emplace(*script, allocator);

				const char* path = serializer.readString();
				serializer.read(scr.m_flags);
				int prop_count;
				serializer.read(prop_count);
				scr.m_properties.reserve(prop_count);
				for (int j = 0; j < prop_count; ++j)
				{
					Property& prop = scr.m_properties.emplace(allocator);
					prop.type = Property::ANY;
					serializer.read(prop.name_hash);
					Property::Type type;
					serializer.read(type);
					const char* tmp = serializer.readString();
					prop.stored_value = tmp;
				}
				setPath(*script, scr, Path(path));
			}
			m_world.onComponentCreated(script->m_entity, ANGELSCRIPT_TYPE, this);
		}
	}

	ISystem& getSystem() const override { return m_system; }

	void update(float time_delta) override
	{
		PROFILE_FUNCTION();
		if (!m_is_game_running) return;
		// Update script timers, contexts, etc.
	}

	Property& getScriptProperty(EntityRef entity, int scr_index, const char* name)
	{
		const StableHash name_hash(name);
		ScriptComponent* script_cmp = m_scripts[entity];
		for (auto& prop : script_cmp->m_scripts[scr_index].m_properties)
		{
			if (prop.name_hash == name_hash)
			{
				return prop;
			}
		}

		script_cmp->m_scripts[scr_index].m_properties.emplace(m_system.m_allocator);
		auto& prop = script_cmp->m_scripts[scr_index].m_properties.back();
		prop.name_hash = name_hash;
		prop.type = Property::ANY;
		return prop;
	}

	Path getScriptPath(EntityRef entity, int scr_index) override
	{
		auto& tmp = m_scripts[entity]->m_scripts[scr_index];
		return tmp.m_script ? tmp.m_script->getPath() : Path("");
	}

	void setPath(ScriptComponent& cmp, ScriptInstance& inst, const Path& path)
	{
		if (inst.m_script)
		{
			auto& cb = inst.m_script->getObserverCb();
			cb.unbind<&ScriptComponent::onScriptLoaded>(&cmp);
			inst.m_script->decRefCount();
		}

		ResourceManagerHub& rm = m_system.m_engine_ref.getResourceManager();
		inst.m_script = path.isEmpty() ? nullptr : rm.load<ASScript>(path);
		if (inst.m_script) inst.m_script->onLoaded<&ScriptComponent::onScriptLoaded>(&cmp);
	}

	void setScriptPath(EntityRef entity, int scr_index, const Path& path) override
	{
		auto* script_cmp = m_scripts[entity];
		if (script_cmp->m_scripts.size() <= scr_index) return;
		setPath(*script_cmp, script_cmp->m_scripts[scr_index], path);
	}

	int getScriptCount(EntityRef entity) override { return m_scripts[entity]->m_scripts.size(); }

	void insertScript(EntityRef entity, int idx) override
	{
		ScriptComponent* cmp = m_scripts[entity];
		cmp->m_scripts.emplaceAt(idx, *cmp, m_system.m_allocator);
	}

	int addScript(EntityRef entity, int scr_index) override
	{
		ScriptComponent* script_cmp = m_scripts[entity];
		if (scr_index == -1) scr_index = script_cmp->m_scripts.size();
		script_cmp->m_scripts.emplaceAt(scr_index, *script_cmp, m_system.m_allocator);
		return scr_index;
	}

	void moveScript(EntityRef entity, int scr_index, bool up) override
	{
		auto* script_cmp = m_scripts[entity];
		if (!up && scr_index > script_cmp->m_scripts.size() - 2) return;
		if (up && scr_index == 0) return;
		int other = up ? scr_index - 1 : scr_index + 1;
		swap(script_cmp->m_scripts[scr_index], script_cmp->m_scripts[other]);
	}

	void enableScript(EntityRef entity, int scr_index, bool enable) override
	{
		ScriptInstance& inst = m_scripts[entity]->m_scripts[scr_index];
		setFlag(inst.m_flags, ScriptInstance::ENABLED, enable);
	}

	bool isScriptEnabled(EntityRef entity, int scr_index) override
	{
		return m_scripts[entity]->m_scripts[scr_index].m_flags & ScriptInstance::ENABLED;
	}

	void removeScript(EntityRef entity, int scr_index) override { m_scripts[entity]->m_scripts.swapAndPop(scr_index); }

	const char* getInlineScriptCode(EntityRef entity) override { return m_inline_scripts[entity].m_source.c_str(); }

	void setInlineScriptCode(EntityRef entity, const char* value) override
	{
		m_inline_scripts[entity].m_source = value;
	}

	AngelScriptSystemImpl& m_system;
	HashMap<EntityRef, ScriptComponent*> m_scripts;
	HashMap<EntityRef, InlineScriptComponent> m_inline_scripts;
	HashMap<StableHash, String> m_property_names;
	World& m_world;
	FunctionCall m_function_call;
	bool m_is_game_running = false;
};

AngelScriptSystemImpl::AngelScriptSystemImpl(Engine& engine)
	: m_engine_ref(engine)
	, m_allocator(engine.getAllocator(), "angelscript system")
	, m_script_manager(m_allocator)
	, m_string_factory(m_allocator)
	, m_as_resources(m_allocator)
{
	m_engine = asCreateScriptEngine();
	if (!m_engine)
	{
		logError("Failed to create AngelScript engine");
		return;
	}

	// Set message callback
	m_engine->SetMessageCallback(asFUNCTION(messageCallback), nullptr, asCALL_CDECL);

	// Register string factory
	AngelScriptWrapper::registerStringType(m_engine, &m_string_factory);

	// Register basic types
	AngelScriptWrapper::registerBasicTypes(m_engine);
	AngelScriptWrapper::registerMathTypes(m_engine);

	AngelScriptWrapper::registerEntityTypes(m_engine);

	m_script_manager.create(ASScript::TYPE, engine.getResourceManager());

	LUMIX_MODULE(AngelScriptModuleImpl, "angelscript")
		.LUMIX_CMP(InlineScriptComponent, "angelscript_inline", "AngelScript / Inline")
		.LUMIX_PROP(InlineScriptCode, "Code")
		.multilineAttribute()
		.LUMIX_CMP(ScriptComponent, "angelscript", "AngelScript / File")
		.LUMIX_FUNC_EX(AngelScriptModule::getScriptPath, "getScriptPath")
		.begin_array<&AngelScriptModule::getScriptCount,
			&AngelScriptModule::addScript,
			&AngelScriptModule::removeScript>("scripts")
		.prop<&AngelScriptModule::isScriptEnabled, &AngelScriptModule::enableScript>("Enabled")
		.LUMIX_PROP(ScriptPath, "Path")
		.resourceAttribute(ASScript::TYPE)
		.end_array();
}

AngelScriptSystemImpl::~AngelScriptSystemImpl()
{
	for (Resource* res : m_as_resources)
	{
		res->decRefCount();
	}

	if (m_engine)
	{
		m_engine->ShutDownAndRelease();
	}

	m_script_manager.destroy();
}

void AngelScriptSystemImpl::createModules(World& world)
{
	UniquePtr<AngelScriptModuleImpl> module = UniquePtr<AngelScriptModuleImpl>::create(m_allocator, *this, world);

	world.addModule(module.move());
}

LUMIX_PLUGIN_ENTRY(angelscript)
{
	PROFILE_FUNCTION();
	return LUMIX_NEW(engine.getAllocator(), AngelScriptSystemImpl)(engine);
}

} // namespace Lumix