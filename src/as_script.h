#pragma once

#include "core/string.h"
#include "core/tag_allocator.h"
#include "engine/resource.h"

namespace Lumix
{

struct ASScript final : Resource
{
public:
	ASScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator);
	virtual ~ASScript();

	ResourceType getType() const override { return TYPE; }

	void unload() override;
	bool load(Span<const u8> mem) override;
	StringView getSourceCode() const { return m_source_code; }

	static inline const ResourceType TYPE = ResourceType("as_script");

private:
	TagAllocator m_allocator;
	Array<ASScript*> m_dependencies;
	String m_source_code;
};

} // namespace Lumix