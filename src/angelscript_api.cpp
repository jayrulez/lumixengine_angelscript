// start angelscript_api.cpp
#include "angelscript_system.h"
#include "angelscript_wrapper.h"
#include "core/delegate.h"
#include "core/log.h"
#include "core/math.h"
#include "core/os.h"
#include "core/profiler.h"
#include "core/string.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/input_system.h"
#include "engine/plugin.h"
#include "engine/prefab.h"
#include "engine/reflection.h"
#include "engine/world.h"
#include <angelscript.h>

namespace Lumix
{

// Forward declarations for reflection API
static void registerComponentProperty(asIScriptEngine* engine,
	const char* component_name,
	const reflection::PropertyBase* prop);
static void registerComponentMethod(asIScriptEngine* engine,
	const char* component_name,
	const reflection::FunctionBase* func);
static void registerModuleMethod(asIScriptEngine* engine,
	const char* module_name,
	const reflection::FunctionBase* func);
static const char* getAngelScriptTypeName(const reflection::TypeDescriptor& type);
static float getFloatProperty(void* component);
static void setFloatProperty(void* component, float value);
static i32 getIntProperty(void* component);
static void setIntProperty(void* component, i32 value);
static u32 getU32Property(void* component);
static void setU32Property(void* component, u32 value);
static bool getBoolProperty(void* component);
static void setBoolProperty(void* component, bool value);
static Vec2 getVec2Property(void* component);
static void setVec2Property(void* component, const Vec2& value);
static Vec3 getVec3Property(void* component);
static void setVec3Property(void* component, const Vec3& value);
static Vec4 getVec4Property(void* component);
static void setVec4Property(void* component, const Vec4& value);
static i32 getIVec3PropertyX(void* component);
static i32 getIVec3PropertyY(void* component);
static i32 getIVec3PropertyZ(void* component);
static void setIVec3Property(void* component, i32 x, i32 y, i32 z);
static EntityRef getEntityProperty(void* component);
static void setEntityProperty(void* component, const EntityRef& value);
static void getPathProperty(void* component, String& out);
static void setPathProperty(void* component, const String& value);
static void getStringProperty(void* component, String& out);
static void setStringProperty(void* component, const String& value);
static u32 getArrayPropertyCount(void* component);
static void addArrayPropertyItem(void* component);
static void removeArrayPropertyItem(void* component, u32 index);

// Entity wrapper functions
static void AS_createComponent(World* world, int entity, const String& type)
{
	if (!world) return;
	ComponentType cmp_type = reflection::getComponentType(type.c_str());
	IModule* module = world->getModule(cmp_type);
	if (!module) return;
	if (world->hasComponent({entity}, cmp_type))
	{
		logError("Component ", type, " already exists in entity ", entity);
		return;
	}

	world->createComponent(cmp_type, {entity});
}

static bool AS_hasComponent(World* world, int entity, const String& type)
{
	if (!world) return false;
	ComponentType cmp_type = reflection::getComponentType(type.c_str());
	return world->hasComponent({entity}, cmp_type);
}

static EntityRef AS_createEntity(World* world)
{
	return world->createEntity({0, 0, 0}, Quat::IDENTITY);
}

static void AS_destroyEntity(World* world, int entity)
{
	world->destroyEntity({entity});
}

static void AS_setEntityPosition(World* world, int entity, const DVec3& pos)
{
	world->setPosition({entity}, pos);
}

static DVec3 AS_getEntityPosition(World* world, int entity)
{
	return world->getPosition({entity});
}

static void AS_setEntityRotation(World* world, int entity, const Quat& rot)
{
	world->setRotation({entity}, rot);
}

static Quat AS_getEntityRotation(World* world, int entity)
{
	return world->getRotation({entity});
}

static void AS_setEntityScale(World* world, int entity, const Vec3& scale)
{
	world->setScale({entity}, scale);
}

static Vec3 AS_getEntityScale(World* world, int entity)
{
	return world->getScale({entity});
}

static int AS_getFirstChild(World* world, int entity)
{
	return world->getFirstChild({entity}).index;
}

static int AS_getParent(World* world, int entity)
{
	return world->getParent({entity}).index;
}

static int AS_findByName(World* world, int entity, const String& name)
{
	return world->findByName(EntityPtr{entity}, name.c_str()).index;
}

static void AS_setParent(World* world, int parent, int child)
{
	world->setParent(EntityPtr{parent}, EntityRef{child});
}

static void AS_setEntityName(World* world, int entity, const String& name)
{
	world->setEntityName({entity}, name.c_str());
}

static void AS_getEntityName(World* world, int entity, String& out)
{
	const char* name = world->getEntityName({entity});
	out = name ? name : "";
}

// File system functions
static bool AS_writeFile(Engine* engine, const String& path, const String& content)
{
	FileSystem& fs = engine->getFileSystem();
	os::OutputFile file;
	if (!fs.open(path.c_str(), file))
	{
		return false;
	}

	bool res = file.write(content.c_str(), content.length());
	file.close();
	return res;
}

static void AS_pause(Engine* engine, bool pause)
{
	engine->pause(pause);
}

static bool AS_hasFilesystemWork(Engine* engine)
{
	return engine->getFileSystem().hasWork();
}

static void AS_processFilesystemWork(Engine* engine)
{
	engine->getFileSystem().processCallbacks();
}

// Logging functions
static void AS_logError(const String& text)
{
	logError(text.c_str());
}

static void AS_logInfo(const String& text)
{
	logInfo(text.c_str());
}

// Engine functions
static void AS_setTimeMultiplier(Engine* engine, float multiplier)
{
	engine->setTimeMultiplier(multiplier);
}

static void AS_startGame(Engine* engine, World* world)
{
	if (engine && world) engine->startGame(*world);
}

// World functions
World* AS_createWorld(Engine* engine)
{
	return &engine->createWorld();
}

static void AS_destroyWorld(Engine* engine, World* world)
{
	engine->destroyWorld(*world);
}

static void AS_setActivePartition(World* world, u16 partition)
{
	world->setActivePartition(World::PartitionHandle(partition));
}

static u16 AS_createPartition(World* world, const String& name)
{
	return (u16)world->createPartition(name.c_str());
}

static u16 AS_getActivePartition(World* world)
{
	return (u16)world->getActivePartition();
}

// Resource functions
static int AS_loadResource(AngelScriptSystem* system, const String& path, const String& type)
{
	return system->addASResource(Path(path.c_str()), ResourceType(type.c_str()));
}

static void AS_getResourcePath(AngelScriptSystem* system, int resource_handle, String& out)
{
	Resource* res = system->getASResource(resource_handle);
	out = res ? res->getPath().c_str() : "";
}

static void AS_unloadResource(AngelScriptSystem* system, int resource_idx)
{
	system->unloadASResource(resource_idx);
}

// Network functions (simplified stubs for now)
static int AS_networkConnect(const String& ip, u16 port)
{
	// TODO: Implement network connection
	return -1;
}

static int AS_networkListen(const String& ip, u16 port)
{
	// TODO: Implement network listening
	return -1;
}

static void AS_networkClose(int stream)
{
	// TODO: Implement network close
}

static bool AS_networkWrite(int stream, const String& data)
{
	// TODO: Implement network write
	return false;
}

static void AS_networkRead(int stream, u32 size, String& out)
{
	// TODO: Implement network read
	out = "";
}

// Input system functions
static bool AS_isKeyPressed(InputSystem* input, int keycode)
{
	// TODO: Implement input checking
	return false;
}

static bool AS_isMouseButtonPressed(InputSystem* input, int button)
{
	// TODO: Implement mouse button checking
	return false;
}

// Math utility functions
static float AS_sin(float x)
{
	return sinf(x);
}
static float AS_cos(float x)
{
	return cosf(x);
}
static float AS_tan(float x)
{
	return tanf(x);
}
static float AS_sqrt(float x)
{
	return sqrtf(x);
}
static float AS_abs(float x)
{
	return fabsf(x);
}
static float AS_pow(float x, float y)
{
	return powf(x, y);
}
static float AS_floor(float x)
{
	return floorf(x);
}
static float AS_ceil(float x)
{
	return ceilf(x);
}
static float AS_min(float a, float b)
{
	return minimum(a, b);
}
static float AS_max(float a, float b)
{
	return maximum(a, b);
}

// Register all engine API functions
void registerEngineAPI(asIScriptEngine* engine, Engine* lumix_engine, AngelScriptSystem* as_system)
{
	int r;

	// Register Engine functions
	r = engine->RegisterGlobalFunction("World@ createWorld()", asFUNCTION(AS_createWorld), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void destroyWorld(World@)", asFUNCTION(AS_destroyWorld), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void pause(bool)", asFUNCTION(AS_pause), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("bool hasFilesystemWork()", asFUNCTION(AS_hasFilesystemWork), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction(
		"void processFilesystemWork()", asFUNCTION(AS_processFilesystemWork), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction(
		"bool writeFile(const String &in, const String &in)", asFUNCTION(AS_writeFile), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void setTimeMultiplier(float)", asFUNCTION(AS_setTimeMultiplier), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void startGame(World@)", asFUNCTION(AS_startGame), asCALL_CDECL);
	ASSERT(r >= 0);

	// Register World type and functions
	r = engine->RegisterObjectType("World", 0, asOBJ_REF | asOBJ_NOCOUNT);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void createComponent(int, const String &in)", asFUNCTION(AS_createComponent), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "bool hasComponent(int, const String &in)", asFUNCTION(AS_hasComponent), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "Entity createEntity()", asFUNCTION(AS_createEntity), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void destroyEntity(int)", asFUNCTION(AS_destroyEntity), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("World",
		"void setEntityPosition(int, const DVec3 &in)",
		asFUNCTION(AS_setEntityPosition),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "DVec3 getEntityPosition(int)", asFUNCTION(AS_getEntityPosition), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("World",
		"void setEntityRotation(int, const Quat &in)",
		asFUNCTION(AS_setEntityRotation),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "Quat getEntityRotation(int)", asFUNCTION(AS_getEntityRotation), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void setEntityScale(int, const Vec3 &in)", asFUNCTION(AS_setEntityScale), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "Vec3 getEntityScale(int)", asFUNCTION(AS_getEntityScale), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "int getFirstChild(int)", asFUNCTION(AS_getFirstChild), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("World", "int getParent(int)", asFUNCTION(AS_getParent), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "int findByName(int, const String &in)", asFUNCTION(AS_findByName), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void setParent(int, int)", asFUNCTION(AS_setParent), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void setEntityName(int, const String &in)", asFUNCTION(AS_setEntityName), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "String getEntityName(int)", asFUNCTION(AS_getEntityName), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "void setActivePartition(uint16)", asFUNCTION(AS_setActivePartition), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "uint16 createPartition(const String &in)", asFUNCTION(AS_createPartition), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"World", "uint16 getActivePartition()", asFUNCTION(AS_getActivePartition), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);

	// Register logging functions
	r = engine->RegisterGlobalFunction("void logError(const String &in)", asFUNCTION(AS_logError), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void logInfo(const String &in)", asFUNCTION(AS_logInfo), asCALL_CDECL);
	ASSERT(r >= 0);

	// Register resource functions
	r = engine->RegisterGlobalFunction(
		"int loadResource(const String &in, const String &in)", asFUNCTION(AS_loadResource), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("String getResourcePath(int)", asFUNCTION(AS_getResourcePath), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void unloadResource(int)", asFUNCTION(AS_unloadResource), asCALL_CDECL);
	ASSERT(r >= 0);

	// Register network functions (stubs)
	r = engine->RegisterGlobalFunction(
		"int networkConnect(const String &in, uint16)", asFUNCTION(AS_networkConnect), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction(
		"int networkListen(const String &in, uint16)", asFUNCTION(AS_networkListen), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void networkClose(int)", asFUNCTION(AS_networkClose), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction(
		"bool networkWrite(int, const String &in)", asFUNCTION(AS_networkWrite), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("String networkRead(int, uint32)", asFUNCTION(AS_networkRead), asCALL_CDECL);
	ASSERT(r >= 0);

	// Register math functions
	r = engine->RegisterGlobalFunction("float sin(float)", asFUNCTION(AS_sin), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float cos(float)", asFUNCTION(AS_cos), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float tan(float)", asFUNCTION(AS_tan), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float sqrt(float)", asFUNCTION(AS_sqrt), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float abs(float)", asFUNCTION(AS_abs), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float pow(float, float)", asFUNCTION(AS_pow), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float floor(float)", asFUNCTION(AS_floor), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float ceil(float)", asFUNCTION(AS_ceil), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float min(float, float)", asFUNCTION(AS_min), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float max(float, float)", asFUNCTION(AS_max), asCALL_CDECL);
	ASSERT(r >= 0);

	// Register input system functions (stubs)
	r = engine->RegisterObjectType("InputSystem", 0, asOBJ_REF | asOBJ_NOCOUNT);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"InputSystem", "bool isKeyPressed(int)", asFUNCTION(AS_isKeyPressed), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"InputSystem", "bool isMouseButtonPressed(int)", asFUNCTION(AS_isMouseButtonPressed), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);

	// Register constants
	r = engine->RegisterGlobalProperty("const int INVALID_ENTITY", (void*)&INVALID_ENTITY);
	ASSERT(r >= 0);

	// Register key codes (basic set)
	int key_escape = (int)os::Keycode::ESCAPE;
	int key_space = (int)os::Keycode::SPACE;
	int key_enter = (int)os::Keycode::RETURN;
	r = engine->RegisterGlobalProperty("const int KEY_ESCAPE", &key_escape);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("const int KEY_SPACE", &key_space);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("const int KEY_ENTER", &key_enter);
	ASSERT(r >= 0);

	// Set user data pointers for use in global functions
	engine->SetUserData(lumix_engine, 0);
	engine->SetUserData(as_system, 1);
}

// Register component-specific API functions
void registerComponentAPI(asIScriptEngine* engine)
{
	int r;

	// Register common component types as enums or constants
	r = engine->RegisterGlobalProperty("const String TRANSFORM_COMPONENT", (void*)"transform");
	ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("const String MESH_COMPONENT", (void*)"mesh");
	ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("const String CAMERA_COMPONENT", (void*)"camera");
	ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("const String LIGHT_COMPONENT", (void*)"light");
	ASSERT(r >= 0);

	// TODO: Add more specific component API functions as needed
}

// Helper function to convert AngelScript generic args to reflection::Variant
static void toVariant(asIScriptGeneric* gen, int arg_idx, reflection::Variant::Type type, reflection::Variant& val)
{
	switch (type)
	{
		case reflection::Variant::BOOL: val = gen->GetArgByte(arg_idx) != 0; break;
		case reflection::Variant::U32: val = (u32)gen->GetArgDWord(arg_idx); break;
		case reflection::Variant::I32: val = (i32)gen->GetArgDWord(arg_idx); break;
		case reflection::Variant::FLOAT: val = gen->GetArgFloat(arg_idx); break;
		case reflection::Variant::ENTITY:
		{
			EntityRef* entity = static_cast<EntityRef*>(gen->GetArgAddress(arg_idx));
			val = EntityPtr{entity->index};
			break;
		}
		case reflection::Variant::VEC2:
		{
			Vec2* vec = static_cast<Vec2*>(gen->GetArgAddress(arg_idx));
			val = *vec;
			break;
		}
		case reflection::Variant::COLOR:
		case reflection::Variant::VEC3:
		{
			Vec3* vec = static_cast<Vec3*>(gen->GetArgAddress(arg_idx));
			val = *vec;
			break;
		}
		case reflection::Variant::DVEC3:
		{
			DVec3* vec = static_cast<DVec3*>(gen->GetArgAddress(arg_idx));
			val = *vec;
			break;
		}
		case reflection::Variant::QUAT:
		{
			Quat* quat = static_cast<Quat*>(gen->GetArgAddress(arg_idx));
			val = *quat;
			break;
		}
		case reflection::Variant::CSTR:
		{
			String* str = static_cast<String*>(gen->GetArgAddress(arg_idx));
			val = str->c_str();
			break;
		}
		case reflection::Variant::PTR:
		{
			void* ptr = gen->GetArgAddress(arg_idx);
			val = ptr;
			break;
		}
		case reflection::Variant::VOID: break;
	}
}

// Helper function to set AngelScript return value from reflection::Variant
static void fromVariant(asIScriptGeneric* gen, Span<u8> val, const reflection::TypeDescriptor& type)
{
	switch (type.type)
	{
		case reflection::Variant::VOID: break;
		case reflection::Variant::BOOL:
		{
			bool v;
			memcpy(&v, val.m_begin, sizeof(v));
			gen->SetReturnByte(v ? 1 : 0);
			break;
		}
		case reflection::Variant::U32:
		{
			u32 v;
			memcpy(&v, val.m_begin, sizeof(v));
			gen->SetReturnDWord(v);
			break;
		}
		case reflection::Variant::I32:
		{
			i32 v;
			memcpy(&v, val.m_begin, sizeof(v));
			gen->SetReturnDWord((u32)v);
			break;
		}
		case reflection::Variant::FLOAT:
		{
			float v;
			memcpy(&v, val.m_begin, sizeof(v));
			gen->SetReturnFloat(v);
			break;
		}
		case reflection::Variant::VEC2:
		{
			Vec2 v;
			memcpy(&v, val.m_begin, sizeof(v));
			// For value types, we need to return by copying to the return location
			Vec2* ret_addr = static_cast<Vec2*>(gen->GetAddressOfReturnLocation());
			*ret_addr = v;
			break;
		}
		case reflection::Variant::COLOR:
		case reflection::Variant::VEC3:
		{
			Vec3 v;
			memcpy(&v, val.m_begin, sizeof(v));
			Vec3* ret_addr = static_cast<Vec3*>(gen->GetAddressOfReturnLocation());
			*ret_addr = v;
			break;
		}
		case reflection::Variant::DVEC3:
		{
			DVec3 v;
			memcpy(&v, val.m_begin, sizeof(v));
			DVec3* ret_addr = static_cast<DVec3*>(gen->GetAddressOfReturnLocation());
			*ret_addr = v;
			break;
		}
		case reflection::Variant::QUAT:
		{
			Quat v;
			memcpy(&v, val.m_begin, sizeof(v));
			Quat* ret_addr = static_cast<Quat*>(gen->GetAddressOfReturnLocation());
			*ret_addr = v;
			break;
		}
		case reflection::Variant::ENTITY:
		{
			EntityPtr v;
			memcpy(&v, val.m_begin, sizeof(v));
			EntityRef* ret_addr = static_cast<EntityRef*>(gen->GetAddressOfReturnLocation());
			*ret_addr = EntityRef{v.index};
			break;
		}
		case reflection::Variant::CSTR:
		{
			const char* v;
			memcpy(&v, val.m_begin, sizeof(v));
			String* ret_addr = static_cast<String*>(gen->GetAddressOfReturnLocation());
			*ret_addr = v ? v : "";
			break;
		}
		case reflection::Variant::PTR:
		{
			void* v;
			memcpy(&v, val.m_begin, sizeof(v));
			gen->SetReturnAddress(v);
			break;
		}
	}
}

// Helper function for component method calls
static void componentMethodClosure(asIScriptGeneric* gen)
{
	//asIScriptContext* ctx = gen->GetContext();

	// Get the reflection function from auxiliary data
	reflection::FunctionBase* f = static_cast<reflection::FunctionBase*>(gen->GetAuxiliary());

	// Get the component instance (this pointer)
	void* obj = gen->GetObject();

	// Convert arguments
	reflection::Variant args[32];
	ASSERT(f->getArgCount() <= lengthOf(args));

	// First argument is always the entity for component methods
	EntityRef* entity_ptr = static_cast<EntityRef*>(gen->GetAddressOfArg(0));
	args[0] = EntityPtr{entity_ptr->index};

	// Convert remaining arguments
	for (u32 i = 1; i < f->getArgCount(); ++i)
	{
		reflection::Variant::Type type = f->getArgType(i).type;
		toVariant(gen, i - 1, type, args[i]); // -1 because AngelScript args don't include 'this'
	}

	// Prepare return value storage
	u8 res_mem[sizeof(DVec3)]; // Largest possible return type
	reflection::TypeDescriptor ret_type = f->getReturnType();
	ASSERT(ret_type.size <= sizeof(res_mem));
	Span<u8> res(res_mem, ret_type.size);

	// Call the function
	f->invoke(obj, res, Span(args, f->getArgCount()));

	// Set return value
	if (ret_type.type != reflection::Variant::VOID)
	{
		fromVariant(gen, res, ret_type);
	}
}

// Helper function for module method calls
static void moduleMethodClosure(asIScriptGeneric* gen)
{
	// Get the reflection function from auxiliary data
	reflection::FunctionBase* f = static_cast<reflection::FunctionBase*>(gen->GetAuxiliary());

	// Get the module instance
	IModule* module = static_cast<IModule*>(gen->GetObject());

	// Convert arguments
	reflection::Variant args[32];
	ASSERT(f->getArgCount() <= lengthOf(args));
	for (u32 i = 0; i < f->getArgCount(); ++i)
	{
		reflection::Variant::Type type = f->getArgType(i).type;
		toVariant(gen, i, type, args[i]);
	}

	// Prepare return value storage
	u8 res_mem[sizeof(DVec3)];
	reflection::TypeDescriptor ret_type = f->getReturnType();
	ASSERT(ret_type.size <= sizeof(res_mem));
	Span<u8> res(res_mem, ret_type.size);

	// Call the function
	f->invoke(module, res, Span(args, f->getArgCount()));

	// Set return value
	if (ret_type.type != reflection::Variant::VOID)
	{
		fromVariant(gen, res, ret_type);
	}
}

// Register reflection API for dynamic component access
void registerReflectionAPI(asIScriptEngine* engine)
{
	int r;

	// Register basic reflection types
	r = engine->RegisterObjectType("ComponentType", sizeof(ComponentType), asOBJ_VALUE | asOBJ_POD);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("ComponentType", "int32 index", asOFFSET(ComponentType, index));
	ASSERT(r >= 0);

	// Register all components from reflection database
	for (const reflection::RegisteredComponent& cmp : reflection::getComponents())
	{
		const char* cmp_name = cmp.cmp->name;

		// Register component as reference type (since it's managed by the world)
		r = engine->RegisterObjectType(cmp_name, 0, asOBJ_REF | asOBJ_NOCOUNT);
		ASSERT(r >= 0);

		// Register component properties
		for (const reflection::PropertyBase* prop : cmp.cmp->props)
		{
			registerComponentProperty(engine, cmp_name, prop);
		}

		// Register component methods
		for (const reflection::FunctionBase* func : cmp.cmp->functions)
		{
			registerComponentMethod(engine, cmp_name, func);
		}
	}

	// Register module functions
	reflection::Module* module = reflection::getFirstModule();
	while (module)
	{
		// Register module as reference type
		r = engine->RegisterObjectType(module->name, 0, asOBJ_REF | asOBJ_NOCOUNT);
		ASSERT(r >= 0);

		// Register module functions
		for (const reflection::FunctionBase* func : module->functions)
		{
			registerModuleMethod(engine, module->name, func);
		}

		module = module->next;
	}
}

// Helper function to register component properties
static void registerComponentProperty(asIScriptEngine* engine,
	const char* component_name,
	const reflection::PropertyBase* prop)
{
	// This is a simplified implementation - would need full property visitor pattern
	struct PropertyVisitor : reflection::IPropertyVisitor
	{
		PropertyVisitor(asIScriptEngine* engine, const char* cmp_name, const char* prop_name)
			: engine(engine)
			, cmp_name(cmp_name)
			, prop_name(prop_name)
		{
		}

		void visit(const reflection::Property<float>& prop) override
		{
			StaticString<256> decl(cmp_name, " float get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getFloatProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl(cmp_name, " void set_", prop_name, "(float)");
				engine->RegisterObjectMethod(
					cmp_name, setter_decl, asFUNCTION(setFloatProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<int>& prop) override
		{
			StaticString<256> decl(cmp_name, " int32 get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getIntProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl(cmp_name, " void set_", prop_name, "(int32)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setIntProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<bool>& prop) override
		{
			StaticString<256> decl(cmp_name, " bool get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getBoolProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl(cmp_name, " void set_", prop_name, "(bool)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setBoolProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<Vec3>& prop) override
		{
			StaticString<256> decl(cmp_name, " Vec3 get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getVec3Property), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl(cmp_name, " void set_", prop_name, "(const Vec3 &in)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setVec3Property), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<EntityPtr>& prop) override
		{
			StaticString<256> decl(cmp_name, " Entity get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getEntityProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl(cmp_name, " void set_", prop_name, "(const Entity &in)");
				engine->RegisterObjectMethod(
					cmp_name, setter_decl, asFUNCTION(setEntityProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<u32>& prop) override
		{
			StaticString<256> decl("uint32 get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getU32Property), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(uint32)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setU32Property), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<Vec2>& prop) override
		{
			StaticString<256> decl("Vec2 get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getVec2Property), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(const Vec2 &in)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setVec2Property), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<IVec3>& prop) override
		{
			// Register as separate getters for x, y, z since AngelScript doesn't have IVec3
			StaticString<256> decl_x("int32 get_", prop_name, "_x()");
			StaticString<256> decl_y("int32 get_", prop_name, "_y()");
			StaticString<256> decl_z("int32 get_", prop_name, "_z()");
			engine->RegisterObjectMethod(cmp_name, decl_x, asFUNCTION(getIVec3PropertyX), asCALL_CDECL_OBJFIRST);
			engine->RegisterObjectMethod(cmp_name, decl_y, asFUNCTION(getIVec3PropertyY), asCALL_CDECL_OBJFIRST);
			engine->RegisterObjectMethod(cmp_name, decl_z, asFUNCTION(getIVec3PropertyZ), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(int32, int32, int32)");
				engine->RegisterObjectMethod(
					cmp_name, setter_decl, asFUNCTION(setIVec3Property), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<Vec4>& prop) override
		{
			StaticString<256> decl("Vec4 get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getVec4Property), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(const Vec4 &in)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setVec4Property), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<Path>& prop) override
		{
			StaticString<256> decl("String get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getPathProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(const String &in)");
				engine->RegisterObjectMethod(cmp_name, setter_decl, asFUNCTION(setPathProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::Property<const char*>& prop) override
		{
			StaticString<256> decl("String get_", prop_name, "()");
			engine->RegisterObjectMethod(cmp_name, decl, asFUNCTION(getStringProperty), asCALL_CDECL_OBJFIRST);

			if (prop.setter)
			{
				StaticString<256> setter_decl("void set_", prop_name, "(const String &in)");
				engine->RegisterObjectMethod(
					cmp_name, setter_decl, asFUNCTION(setStringProperty), asCALL_CDECL_OBJFIRST);
			}
		}

		void visit(const reflection::ArrayProperty& prop) override
		{
			// Register array access methods
			StaticString<256> count_decl("uint32 get_", prop_name, "_count()");
			StaticString<256> add_decl("void ", prop_name, "_add()");
			StaticString<256> remove_decl("void ", prop_name, "_remove(uint32)");

			engine->RegisterObjectMethod(
				cmp_name, count_decl, asFUNCTION(getArrayPropertyCount), asCALL_CDECL_OBJFIRST);
			engine->RegisterObjectMethod(cmp_name, add_decl, asFUNCTION(addArrayPropertyItem), asCALL_CDECL_OBJFIRST);
			engine->RegisterObjectMethod(
				cmp_name, remove_decl, asFUNCTION(removeArrayPropertyItem), asCALL_CDECL_OBJFIRST);
		}

		void visit(const reflection::BlobProperty& prop) override
		{
			// Blob properties are not directly accessible from scripts
			logWarning(
				"Blob property '", prop_name, "' in component '", cmp_name, "' cannot be exposed to AngelScript");
		}

		void visit(const reflection::DynamicProperties& prop) override
		{
			// Dynamic properties need special handling
			logWarning("Dynamic property '",
				prop_name,
				"' in component '",
				cmp_name,
				"' requires special handling for AngelScript");
		}

		asIScriptEngine* engine;
		const char* cmp_name;
		const char* prop_name;
	};

	PropertyVisitor visitor(engine, component_name, prop->name);
	prop->visit(visitor);
}

// Helper function to register component methods
static void registerComponentMethod(asIScriptEngine* engine,
	const char* component_name,
	const reflection::FunctionBase* func)
{
	// Convert function signature to AngelScript declaration
	StaticString<512> decl;

	// Get return type
	reflection::TypeDescriptor ret_type = func->getReturnType();
	const char* ret_type_str = getAngelScriptTypeName(ret_type);

	// Build method declaration
	decl.append(ret_type_str, " ", func->name, "(");

	// Add parameters
	for (u32 i = 1; i < func->getArgCount(); ++i)
	{ // Skip first arg (this pointer)
		if (i > 1) decl.append(", ");
		reflection::TypeDescriptor arg_type = func->getArgType(i);
		const char* arg_type_str = getAngelScriptTypeName(arg_type);
		decl.append(arg_type_str);
	}
	decl.append(")");

	// Register the method with a generic wrapper
	int r = engine->RegisterObjectMethod(
		component_name, decl, asFUNCTION(componentMethodClosure), asCALL_GENERIC, (void*)func);
	ASSERT(r >= 0);
}

// Helper function to register module methods
static void registerModuleMethod(asIScriptEngine* engine, const char* module_name, const reflection::FunctionBase* func)
{
	// Similar to registerComponentMethod but for module functions
	StaticString<512> decl;

	reflection::TypeDescriptor ret_type = func->getReturnType();
	const char* ret_type_str = getAngelScriptTypeName(ret_type);

	decl.append(ret_type_str, " ", func->name, "(");

	for (u32 i = 0; i < func->getArgCount(); ++i)
	{
		if (i > 0) decl.append(", ");
		reflection::TypeDescriptor arg_type = func->getArgType(i);
		const char* arg_type_str = getAngelScriptTypeName(arg_type);
		decl.append(arg_type_str);
	}
	decl.append(")");

	int r =
		engine->RegisterObjectMethod(module_name, decl, asFUNCTION(moduleMethodClosure), asCALL_GENERIC, (void*)func);
	ASSERT(r >= 0);
}

// Helper function to convert Lumix types to AngelScript type names
static const char* getAngelScriptTypeName(const reflection::TypeDescriptor& type)
{
	switch (type.type)
	{
		case reflection::Variant::VOID: return "void";
		case reflection::Variant::BOOL: return "bool";
		case reflection::Variant::I32: return "int32";
		case reflection::Variant::U32: return "uint32";
		case reflection::Variant::FLOAT: return "float";
		case reflection::Variant::VEC2: return "Vec2";
		case reflection::Variant::VEC3: return "Vec3";
		case reflection::Variant::DVEC3: return "DVec3";
		case reflection::Variant::COLOR: return "Vec3"; // Treat color as Vec3
		case reflection::Variant::QUAT: return "Quat";
		case reflection::Variant::ENTITY: return "Entity";
		case reflection::Variant::CSTR: return "String";
		case reflection::Variant::PTR: return "void*"; // Generic pointer
		default: return "void*";
	}
}

// Property getter/setter implementations with proper reflection calls
struct PropertyContext
{
	const reflection::PropertyBase* property;
	ComponentUID component;
	int array_index;
};

static float getFloatProperty(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<float>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setFloatProperty(void* component, float value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<float>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static i32 getIntProperty(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<int>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setIntProperty(void* component, i32 value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<int>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static u32 getU32Property(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<u32>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setU32Property(void* component, u32 value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<u32>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static bool getBoolProperty(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<bool>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setBoolProperty(void* component, bool value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<bool>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static Vec2 getVec2Property(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec2>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setVec2Property(void* component, const Vec2& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec2>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static Vec3 getVec3Property(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec3>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setVec3Property(void* component, const Vec3& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec3>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static Vec4 getVec4Property(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec4>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index);
}

static void setVec4Property(void* component, const Vec4& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Vec4>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value);
}

static i32 getIVec3PropertyX(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<IVec3>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index).x;
}

static i32 getIVec3PropertyY(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<IVec3>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index).y;
}

static i32 getIVec3PropertyZ(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<IVec3>*>(ctx->property);
	return prop->get(ctx->component, ctx->array_index).z;
}

static void setIVec3Property(void* component, i32 x, i32 y, i32 z)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<IVec3>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, IVec3{x, y, z});
}

static EntityRef getEntityProperty(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<EntityPtr>*>(ctx->property);
	EntityPtr ptr = prop->get(ctx->component, ctx->array_index);
	return ptr.isValid() ? EntityRef{ptr.index} : EntityRef{-1};
}

static void setEntityProperty(void* component, const EntityRef& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<EntityPtr>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, EntityPtr{value.index});
}

static void getPathProperty(void* component, String& out)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Path>*>(ctx->property);
	Path path = prop->get(ctx->component, ctx->array_index);
	out = path.c_str();
}

static void setPathProperty(void* component, const String& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<Path>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, Path(value.c_str()));
}

static void getStringProperty(void* component, String& out)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<const char*>*>(ctx->property);
	const char* str = prop->get(ctx->component, ctx->array_index);
	out = str ? str : "";
}

static void setStringProperty(void* component, const String& value)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::Property<const char*>*>(ctx->property);
	if (prop->setter) prop->set(ctx->component, ctx->array_index, value.c_str());
}

static u32 getArrayPropertyCount(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::ArrayProperty*>(ctx->property);
	return prop->getCount(ctx->component);
}

static void addArrayPropertyItem(void* component)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::ArrayProperty*>(ctx->property);
	prop->addItem(ctx->component, -1);
}

static void removeArrayPropertyItem(void* component, u32 index)
{
	PropertyContext* ctx = static_cast<PropertyContext*>(component);
	const auto* prop = static_cast<const reflection::ArrayProperty*>(ctx->property);
	if (index < prop->getCount(ctx->component))
	{
		prop->removeItem(ctx->component, index);
	}
}

// Main registration function called by the system
void registerAngelScriptAPI(asIScriptEngine* engine, Engine* lumix_engine, AngelScriptSystem* as_system)
{
	// Register all API categories
	registerEngineAPI(engine, lumix_engine, as_system);
	registerComponentAPI(engine);
	registerReflectionAPI(engine);

	logInfo("AngelScript API registered successfully");
}

} // namespace Lumix
// end angelscript_api.cpp