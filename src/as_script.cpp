#include "as_script.h"
#include "core/log.h"
#include "core/stream.h"
#include "engine/file_system.h"
#include "engine/resource_manager.h"

namespace Lumix
{

ASScript::ASScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, m_allocator(allocator, m_path.c_str())
	, m_source_code(m_allocator)
	, m_dependencies(m_allocator)
{
}

ASScript::~ASScript() = default;

void ASScript::unload()
{
	for (ASScript* scr : m_dependencies) scr->decRefCount();
	m_dependencies.clear();
	m_source_code = "";
}

bool ASScript::load(Span<const u8> mem)
{
	InputMemoryStream blob(mem.begin(), mem.length());
	u32 num_deps;
	blob.read(num_deps);
	for (u32 i = 0; i < num_deps; ++i)
	{
		const char* dep_path = blob.readString();
		ASScript* scr = m_resource_manager.getOwner().load<ASScript>(Path(dep_path));
		addDependency(*scr);
		m_dependencies.push(scr);
	}
	m_source_code = StringView((const char*)blob.skip(0), (u32)blob.remaining());
	return true;
}

} // namespace Lumix