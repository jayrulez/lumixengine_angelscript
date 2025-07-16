#define LUMIX_NO_CUSTOM_CRT

#include "core/crt.h"
#include "core/allocator.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/math.h"
#include "core/string.h"
#include "engine/world.h"
#include "angelscript_wrapper.h"
#include <new>

namespace Lumix
{
namespace AngelScriptWrapper
{

// StringFactory implementation
StringFactory::StringFactory(IAllocator& allocator)
	: m_strings(allocator)
	, m_allocator(allocator)
{
}

StringFactory::~StringFactory()
{
	for (auto iter : m_strings.iterated())
	{
		LUMIX_DELETE(m_allocator, iter.value());
	}
}

const void* StringFactory::GetStringConstant(const char* data, asUINT length)
{
	auto str = (StringView(data, length));
	const StableHash hash = StableHash(str.begin, str.size());
	auto iter = m_strings.find(hash);
	if (iter.isValid())
	{
		iter.value()->ref_count++;
		return &iter.value()->string;
	}

	StringData* str_data = LUMIX_NEW(m_allocator, StringData)(data, length, m_allocator);
	m_strings.insert(hash, str_data);
	return &str_data->string;
}

int StringFactory::ReleaseStringConstant(const void* str)
{
	// Find the string data by comparing the string object address
	for (auto iter : m_strings.iterated())
	{
		if (&iter.value()->string == str)
		{
			iter.value()->ref_count--;
			if (iter.value()->ref_count == 0)
			{
				LUMIX_DELETE(m_allocator, iter.value());
				m_strings.erase(iter);
			}
			return 0;
		}
	}
	return -1;
}

int StringFactory::GetRawStringData(const void* str, char* data, asUINT* length) const
{
	const String* string = static_cast<const String*>(str);
	if (data == nullptr)
	{
		*length = string->length();
	}
	else
	{
		const u32 len = minimum(*length, string->length());
		memcpy(data, string->c_str(), len);
		*length = len;
	}
	return 0;
}

// Entity construction/destruction
void EntityDefaultConstructor(void* memory)
{
	new (memory) EntityRef{-1};
}

void EntityCopyConstructor(void* memory, const EntityRef& other)
{
	new (memory) EntityRef(other);
}

void EntityDestructor(void* memory)
{
	static_cast<EntityRef*>(memory)->~EntityRef();
}

// Vec2 construction/destruction
void Vec2DefaultConstructor(void* memory)
{
	new (memory) Vec2();
}

void Vec2CopyConstructor(void* memory, const Vec2& other)
{
	new (memory) Vec2(other);
}

void Vec2InitConstructor(void* memory, float x, float y)
{
	new (memory) Vec2(x, y);
}

void Vec2Destructor(void* memory)
{
	static_cast<Vec2*>(memory)->~Vec2();
}

// Vec3 construction/destruction
void Vec3DefaultConstructor(void* memory)
{
	new (memory) Vec3();
}

void Vec3CopyConstructor(void* memory, const Vec3& other)
{
	new (memory) Vec3(other);
}

void Vec3InitConstructor(void* memory, float x, float y, float z)
{
	new (memory) Vec3(x, y, z);
}

void Vec3Destructor(void* memory)
{
	static_cast<Vec3*>(memory)->~Vec3();
}

// DVec3 construction/destruction
void DVec3DefaultConstructor(void* memory)
{
	new (memory) DVec3();
}

void DVec3CopyConstructor(void* memory, const DVec3& other)
{
	new (memory) DVec3(other);
}

void DVec3InitConstructor(void* memory, double x, double y, double z)
{
	new (memory) DVec3(x, y, z);
}

void DVec3Destructor(void* memory)
{
	static_cast<DVec3*>(memory)->~DVec3();
}

// Vec4 construction/destruction
void Vec4DefaultConstructor(void* memory)
{
	new (memory) Vec4();
}

void Vec4CopyConstructor(void* memory, const Vec4& other)
{
	new (memory) Vec4(other);
}

void Vec4InitConstructor(void* memory, float x, float y, float z, float w)
{
	new (memory) Vec4(x, y, z, w);
}

void Vec4Destructor(void* memory)
{
	static_cast<Vec4*>(memory)->~Vec4();
}

// Quat construction/destruction
void QuatDefaultConstructor(void* memory)
{
	new (memory) Quat();
}

void QuatCopyConstructor(void* memory, const Quat& other)
{
	new (memory) Quat(other);
}

void QuatInitConstructor(void* memory, float x, float y, float z, float w)
{
	new (memory) Quat(x, y, z, w);
}

void QuatDestructor(void* memory)
{
	static_cast<Quat*>(memory)->~Quat();
}

// Entity helper functions
EntityRef* EntityOpAssign(EntityRef* self, const EntityRef& other)
{
	*self = other;
	return self;
}

bool EntityOpEquals(const EntityRef& a, const EntityRef& b)
{
	return a == b;
}

void EntityToString(const EntityRef& entity, String& out)
{
	char tmp[32];
	toCString(entity.index, Span(tmp));
	out = tmp;
}

// Vec2 math operations
Vec2* Vec2OpAssign(Vec2* self, const Vec2& other)
{
	*self = other;
	return self;
}

Vec2 Vec2OpAdd(const Vec2& a, const Vec2& b)
{
	return a + b;
}

Vec2 Vec2OpSub(const Vec2& a, const Vec2& b)
{
	return a - b;
}

Vec2 Vec2OpMul(const Vec2& a, float scalar)
{
	return a * scalar;
}

Vec2 Vec2OpDiv(const Vec2& a, float scalar)
{
	return a / scalar;
}

bool Vec2OpEquals(const Vec2& a, const Vec2& b)
{
	return a.x == b.x && a.y == b.y;
}

// Vec3 math operations
Vec3* Vec3OpAssign(Vec3* self, const Vec3& other)
{
	*self = other;
	return self;
}

Vec3 Vec3OpAdd(const Vec3& a, const Vec3& b)
{
	return a + b;
}

Vec3 Vec3OpSub(const Vec3& a, const Vec3& b)
{
	return a - b;
}

Vec3 Vec3OpMul(const Vec3& a, float scalar)
{
	return a * scalar;
}

Vec3 Vec3OpDiv(const Vec3& a, float scalar)
{
	return a / scalar;
}

bool Vec3OpEquals(const Vec3& a, const Vec3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

float Vec3Dot(const Vec3& a, const Vec3& b)
{
	return dot(a, b);
}

Vec3 Vec3Cross(const Vec3& a, const Vec3& b)
{
	return cross(a, b);
}

float Vec3Length(const Vec3& v)
{
	return length(v);
}

Vec3 Vec3Normalize(const Vec3& v)
{
	return normalize(v);
}

// DVec3 math operations
DVec3* DVec3OpAssign(DVec3* self, const DVec3& other)
{
	*self = other;
	return self;
}

DVec3 DVec3OpAdd(const DVec3& a, const DVec3& b)
{
	return a + b;
}

DVec3 DVec3OpSub(const DVec3& a, const DVec3& b)
{
	return a - b;
}

DVec3 DVec3OpMul(const DVec3& a, double scalar)
{
	return a * (float)scalar;
}

DVec3 DVec3OpDiv(const DVec3& a, double scalar)
{
	return a / (float)scalar;
}

bool DVec3OpEquals(const DVec3& a, const DVec3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Vec4 math operations
Vec4* Vec4OpAssign(Vec4* self, const Vec4& other)
{
	*self = other;
	return self;
}

Vec4 Vec4OpAdd(const Vec4& a, const Vec4& b)
{
	return a + b;
}

Vec4 Vec4OpSub(const Vec4& a, const Vec4& b)
{
	return a - b;
}

Vec4 Vec4OpMul(const Vec4& a, float scalar)
{
	return a * scalar;
}

Vec4 Vec4OpDiv(const Vec4& a, float scalar)
{
	return a / scalar;
}

bool Vec4OpEquals(const Vec4& a, const Vec4& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// Quat math operations
Quat* QuatOpAssign(Quat* self, const Quat& other)
{
	*self = other;
	return self;
}

Quat QuatOpMul(const Quat& a, const Quat& b)
{
	return a * b;
}

bool QuatOpEquals(const Quat& a, const Quat& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

Vec3 QuatRotateVec3(const Quat& q, const Vec3& v)
{
	return q.rotate(v);
}

// Utility functions
void logError(const String& message)
{
	Lumix::logError(message.c_str());
}

void logInfo(const String& message)
{
	Lumix::logInfo(message.c_str());
}

// Registration functions
void registerBasicTypes(asIScriptEngine* engine)
{
	//int r;

	// Register primitive types
	//r = engine->RegisterObjectType("int32", sizeof(i32), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
	//ASSERT(r >= 0);
	//r = engine->RegisterObjectType("uint32", sizeof(u32), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
	//ASSERT(r >= 0);
	//r = engine->RegisterObjectType("int64", sizeof(i64), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
	//ASSERT(r >= 0);
	//r = engine->RegisterObjectType("uint64", sizeof(u64), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);
	//ASSERT(r >= 0);
}

void registerMathTypes(asIScriptEngine* engine)
{
	int r;

	// Register Vec2
	r = engine->RegisterObjectType("Vec2", sizeof(Vec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2DefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec2", asBEHAVE_CONSTRUCT, "void f(const Vec2 &in)", asFUNCTION(Vec2CopyConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(Vec2InitConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec2", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec2Destructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec2", "Vec2& opAssign(const Vec2 &in)", asFUNCTION(Vec2OpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec2", "Vec2 opAdd(const Vec2 &in) const", asFUNCTION(Vec2OpAdd), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec2", "Vec2 opSub(const Vec2 &in) const", asFUNCTION(Vec2OpSub), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec2", "Vec2 opMul(float) const", asFUNCTION(Vec2OpMul), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec2", "Vec2 opDiv(float) const", asFUNCTION(Vec2OpDiv), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec2", "bool opEquals(const Vec2 &in) const", asFUNCTION(Vec2OpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec2", "float x", asOFFSET(Vec2, x));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec2", "float y", asOFFSET(Vec2, y));
	ASSERT(r >= 0);

	// Register Vec3
	r = engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3DefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3CopyConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("Vec3",
		asBEHAVE_CONSTRUCT,
		"void f(float, float, float)",
		asFUNCTION(Vec3InitConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3Destructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "Vec3& opAssign(const Vec3 &in)", asFUNCTION(Vec3OpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "Vec3 opAdd(const Vec3 &in) const", asFUNCTION(Vec3OpAdd), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "Vec3 opSub(const Vec3 &in) const", asFUNCTION(Vec3OpSub), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec3", "Vec3 opMul(float) const", asFUNCTION(Vec3OpMul), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec3", "Vec3 opDiv(float) const", asFUNCTION(Vec3OpDiv), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "bool opEquals(const Vec3 &in) const", asFUNCTION(Vec3OpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "float dot(const Vec3 &in) const", asFUNCTION(Vec3Dot), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "Vec3 cross(const Vec3 &in) const", asFUNCTION(Vec3Cross), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec3", "float length() const", asFUNCTION(Vec3Length), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec3", "Vec3 normalize() const", asFUNCTION(Vec3Normalize), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
	ASSERT(r >= 0);

	// Register DVec3
	r = engine->RegisterObjectType("DVec3", sizeof(DVec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"DVec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(DVec3DefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("DVec3",
		asBEHAVE_CONSTRUCT,
		"void f(const DVec3 &in)",
		asFUNCTION(DVec3CopyConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("DVec3",
		asBEHAVE_CONSTRUCT,
		"void f(double, double, double)",
		asFUNCTION(DVec3InitConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"DVec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DVec3Destructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "DVec3& opAssign(const DVec3 &in)", asFUNCTION(DVec3OpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "DVec3 opAdd(const DVec3 &in) const", asFUNCTION(DVec3OpAdd), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "DVec3 opSub(const DVec3 &in) const", asFUNCTION(DVec3OpSub), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "DVec3 opMul(double) const", asFUNCTION(DVec3OpMul), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "DVec3 opDiv(double) const", asFUNCTION(DVec3OpDiv), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"DVec3", "bool opEquals(const DVec3 &in) const", asFUNCTION(DVec3OpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("DVec3", "double x", asOFFSET(DVec3, x));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("DVec3", "double y", asOFFSET(DVec3, y));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("DVec3", "double z", asOFFSET(DVec3, z));
	ASSERT(r >= 0);

	// Register Vec4
	r = engine->RegisterObjectType("Vec4", sizeof(Vec4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec4DefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec4", asBEHAVE_CONSTRUCT, "void f(const Vec4 &in)", asFUNCTION(Vec4CopyConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("Vec4",
		asBEHAVE_CONSTRUCT,
		"void f(float, float, float, float)",
		asFUNCTION(Vec4InitConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Vec4", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec4Destructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec4", "Vec4& opAssign(const Vec4 &in)", asFUNCTION(Vec4OpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec4", "Vec4 opAdd(const Vec4 &in) const", asFUNCTION(Vec4OpAdd), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec4", "Vec4 opSub(const Vec4 &in) const", asFUNCTION(Vec4OpSub), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec4", "Vec4 opMul(float) const", asFUNCTION(Vec4OpMul), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("Vec4", "Vec4 opDiv(float) const", asFUNCTION(Vec4OpDiv), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Vec4", "bool opEquals(const Vec4 &in) const", asFUNCTION(Vec4OpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec4", "float x", asOFFSET(Vec4, x));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec4", "float y", asOFFSET(Vec4, y));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec4", "float z", asOFFSET(Vec4, z));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Vec4", "float w", asOFFSET(Vec4, w));
	ASSERT(r >= 0);

	// Register Quat
	r = engine->RegisterObjectType("Quat", sizeof(Quat), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Quat", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(QuatDefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Quat", asBEHAVE_CONSTRUCT, "void f(const Quat &in)", asFUNCTION(QuatCopyConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("Quat",
		asBEHAVE_CONSTRUCT,
		"void f(float, float, float, float)",
		asFUNCTION(QuatInitConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Quat", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(QuatDestructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Quat", "Quat& opAssign(const Quat &in)", asFUNCTION(QuatOpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Quat", "Quat opMul(const Quat &in) const", asFUNCTION(QuatOpMul), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Quat", "bool opEquals(const Quat &in) const", asFUNCTION(QuatOpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Quat", "Vec3 rotate(const Vec3 &in) const", asFUNCTION(QuatRotateVec3), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Quat", "float x", asOFFSET(Quat, x));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Quat", "float y", asOFFSET(Quat, y));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Quat", "float z", asOFFSET(Quat, z));
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Quat", "float w", asOFFSET(Quat, w));
	ASSERT(r >= 0);
}

void registerEntityTypes(asIScriptEngine* engine)
{
	int r;

	// Register EntityRef as value type
	r = engine->RegisterObjectType("Entity", sizeof(EntityRef), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Entity", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(EntityDefaultConstructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("Entity",
		asBEHAVE_CONSTRUCT,
		"void f(const Entity &in)",
		asFUNCTION(EntityCopyConstructor),
		asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour(
		"Entity", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(EntityDestructor), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Entity", "Entity& opAssign(const Entity &in)", asFUNCTION(EntityOpAssign), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectMethod(
		"Entity", "bool opEquals(const Entity &in) const", asFUNCTION(EntityOpEquals), asCALL_CDECL_OBJFIRST);
	ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("Entity", "int32 index", asOFFSET(EntityRef, index));
	ASSERT(r >= 0);
}

void registerStringType(asIScriptEngine* engine, StringFactory* string_factory)
{
	int r;

	// Register the String class as reference type
	r = engine->RegisterObjectType("String", sizeof(StringFactory::StringData), asOBJ_VALUE | asOBJ_POD);
	ASSERT(r >= 0);
	r = engine->RegisterStringFactory("String", string_factory);
	ASSERT(r >= 0);

	// Register global functions for logging
	r = engine->RegisterGlobalFunction("void logError(const String &in)", asFUNCTION(logError), asCALL_CDECL);
	ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void logInfo(const String &in)", asFUNCTION(logInfo), asCALL_CDECL);
	ASSERT(r >= 0);
}

} // namespace AngelScriptWrapper
} // namespace Lumix