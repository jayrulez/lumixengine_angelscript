#pragma once

#include "core/math.h"
#include "core/path.h"
#include "core/string.h"
#include "core/hash_map.h"
#include "engine/lumix.h"
#include <angelscript.h>

namespace Lumix
{

struct World;
struct Engine;

namespace AngelScriptWrapper
{

struct StringFactory : public asIStringFactory
{
	StringFactory(IAllocator& allocator);
	~StringFactory();

	const void* GetStringConstant(const char* data, asUINT length) override;
	int ReleaseStringConstant(const void* str) override;
	int GetRawStringData(const void* str, char* data, asUINT* length) const override;

//private:
	struct StringData
	{
		String string;
		i32 ref_count;

		StringData(const char* data, asUINT length, IAllocator& allocator)
			: string(StringView(data, length), allocator)
			, ref_count(1)
		{
		}
	};

	HashMap<StableHash, StringData*> m_strings;
	IAllocator& m_allocator;
};

// Entity construction/destruction helpers
void EntityDefaultConstructor(void* memory);
void EntityCopyConstructor(void* memory, const EntityRef& other);
void EntityDestructor(void* memory);

// Vec2 construction/destruction helpers
void Vec2DefaultConstructor(void* memory);
void Vec2CopyConstructor(void* memory, const Vec2& other);
void Vec2InitConstructor(void* memory, float x, float y);
void Vec2Destructor(void* memory);

// Vec3 construction/destruction helpers
void Vec3DefaultConstructor(void* memory);
void Vec3CopyConstructor(void* memory, const Vec3& other);
void Vec3InitConstructor(void* memory, float x, float y, float z);
void Vec3Destructor(void* memory);

// DVec3 construction/destruction helpers
void DVec3DefaultConstructor(void* memory);
void DVec3CopyConstructor(void* memory, const DVec3& other);
void DVec3InitConstructor(void* memory, double x, double y, double z);
void DVec3Destructor(void* memory);

// Vec4 construction/destruction helpers
void Vec4DefaultConstructor(void* memory);
void Vec4CopyConstructor(void* memory, const Vec4& other);
void Vec4InitConstructor(void* memory, float x, float y, float z, float w);
void Vec4Destructor(void* memory);

// Quat construction/destruction helpers
void QuatDefaultConstructor(void* memory);
void QuatCopyConstructor(void* memory, const Quat& other);
void QuatInitConstructor(void* memory, float x, float y, float z, float w);
void QuatDestructor(void* memory);

// Entity helper functions
EntityRef* EntityOpAssign(EntityRef* self, const EntityRef& other);
bool EntityOpEquals(const EntityRef& a, const EntityRef& b);
void EntityToString(const EntityRef& entity, String& out);

// Math helper functions for Vec2
Vec2* Vec2OpAssign(Vec2* self, const Vec2& other);
Vec2 Vec2OpAdd(const Vec2& a, const Vec2& b);
Vec2 Vec2OpSub(const Vec2& a, const Vec2& b);
Vec2 Vec2OpMul(const Vec2& a, float scalar);
Vec2 Vec2OpDiv(const Vec2& a, float scalar);
bool Vec2OpEquals(const Vec2& a, const Vec2& b);

// Math helper functions for Vec3
Vec3* Vec3OpAssign(Vec3* self, const Vec3& other);
Vec3 Vec3OpAdd(const Vec3& a, const Vec3& b);
Vec3 Vec3OpSub(const Vec3& a, const Vec3& b);
Vec3 Vec3OpMul(const Vec3& a, float scalar);
Vec3 Vec3OpDiv(const Vec3& a, float scalar);
bool Vec3OpEquals(const Vec3& a, const Vec3& b);
float Vec3Dot(const Vec3& a, const Vec3& b);
Vec3 Vec3Cross(const Vec3& a, const Vec3& b);
float Vec3Length(const Vec3& v);
Vec3 Vec3Normalize(const Vec3& v);

// Math helper functions for DVec3
DVec3* DVec3OpAssign(DVec3* self, const DVec3& other);
DVec3 DVec3OpAdd(const DVec3& a, const DVec3& b);
DVec3 DVec3OpSub(const DVec3& a, const DVec3& b);
DVec3 DVec3OpMul(const DVec3& a, double scalar);
DVec3 DVec3OpDiv(const DVec3& a, double scalar);
bool DVec3OpEquals(const DVec3& a, const DVec3& b);

// Math helper functions for Vec4
Vec4* Vec4OpAssign(Vec4* self, const Vec4& other);
Vec4 Vec4OpAdd(const Vec4& a, const Vec4& b);
Vec4 Vec4OpSub(const Vec4& a, const Vec4& b);
Vec4 Vec4OpMul(const Vec4& a, float scalar);
Vec4 Vec4OpDiv(const Vec4& a, float scalar);
bool Vec4OpEquals(const Vec4& a, const Vec4& b);

// Math helper functions for Quat
Quat* QuatOpAssign(Quat* self, const Quat& other);
Quat QuatOpMul(const Quat& a, const Quat& b);
bool QuatOpEquals(const Quat& a, const Quat& b);
Vec3 QuatRotateVec3(const Quat& q, const Vec3& v);

// Utility functions
void logError(const String& message);
void logInfo(const String& message);

// Registration helpers
void registerBasicTypes(asIScriptEngine* engine);
void registerMathTypes(asIScriptEngine* engine);
void registerEntityTypes(asIScriptEngine* engine);
void registerStringType(asIScriptEngine* engine, StringFactory* string_factory);

} // namespace AngelScriptWrapper
} // namespace Lumix