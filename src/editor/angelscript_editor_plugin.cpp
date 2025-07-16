#define LUMIX_NO_CUSTOM_CRT
#include <angelscript.h>
#include <imgui/imgui.h>

#include "../angelscript_system.h"
#include "../angelscript_wrapper.h"
#include "../as_script.h"
#include "core/allocator.h"
#include "core/array.h"
#include "core/command_line_parser.h"
#include "core/crt.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/math.h"
#include "core/os.h"
#include "core/path.h"
#include "core/profiler.h"
#include "core/stream.h"
#include "editor/asset_browser.h"
#include "editor/asset_compiler.h"
#include "editor/editor_asset.h"
#include "editor/property_grid.h"
#include "editor/settings.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "engine/component_uid.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/world.h"
#include "renderer/editor/game_view.h"
#include "renderer/editor/scene_view.h"

using namespace Lumix;

static const ComponentType ANGELSCRIPT_TYPE = reflection::getComponentType("angelscript");

namespace
{

struct StudioAngelScriptPlugin : StudioApp::GUIPlugin
{
	static StudioAngelScriptPlugin* create(StudioApp& app, StringView content, const Path& path)
	{
		AngelScriptSystem* system = (AngelScriptSystem*)app.getEngine().getSystemManager().getSystem("angelscript");
		asIScriptEngine* engine = system->getEngine();

		// Create a temporary module to compile and run the plugin script
		asIScriptModule* module = engine->GetModule("TempPlugin", asGM_CREATE_IF_NOT_EXISTS);

		int r = module->AddScriptSection(path.c_str(), content.begin, content.size());
		if (r < 0)
		{
			logError(path, ": failed to add script section");
			return nullptr;
		}

		r = module->Build();
		if (r < 0)
		{
			logError(path, ": failed to build script");
			return nullptr;
		}

		// Look for plugin initialization function
		asIScriptFunction* init_func = module->GetFunctionByDecl("void initPlugin()");
		if (!init_func)
		{
			logError(path, ": missing initPlugin() function");
			return nullptr;
		}

		// Execute the initialization
		asIScriptContext* ctx = engine->CreateContext();
		ctx->Prepare(init_func);
		r = ctx->Execute();
		if (r != asEXECUTION_FINISHED)
		{
			logError(path, ": failed to execute initPlugin()");
			ctx->Release();
			return nullptr;
		}

		// Get plugin name
		asIScriptFunction* name_func = module->GetFunctionByDecl("string getPluginName()");
		if (!name_func)
		{
			logError(path, ": missing getPluginName() function");
			ctx->Release();
			return nullptr;
		}

		ctx->Prepare(name_func);
		r = ctx->Execute();
		if (r != asEXECUTION_FINISHED)
		{
			logError(path, ": failed to execute getPluginName()");
			ctx->Release();
			return nullptr;
		}

		String* name_ptr = static_cast<String*>(ctx->GetReturnAddress());
		String plugin_name = *name_ptr;

		StudioAngelScriptPlugin* plugin =
			LUMIX_NEW(app.getAllocator(), StudioAngelScriptPlugin)(app, plugin_name.c_str());

		// Check for window menu action
		asIScriptFunction* window_action_func = module->GetFunctionByDecl("void windowMenuAction()");
		if (window_action_func)
		{
			char tmp[64];
			convertToAngelScriptName(plugin_name.c_str(), tmp);
			plugin->m_action.create(plugin_name.c_str(), plugin_name.c_str(), tmp, "", Action::WINDOW);
		}

		plugin->m_script_module = module;
		plugin->m_script_context = ctx;
		app.addPlugin(*plugin);
		return plugin;
	}

	static void convertToAngelScriptName(const char* src, Span<char> out)
	{
		const u32 max_size = out.length();
		ASSERT(max_size > 0);
		char* dest = out.begin();
		while (*src && dest - out.begin() < max_size - 1)
		{
			if (isLetter(*src))
			{
				*dest = isUpperCase(*src) ? *src - 'A' + 'a' : *src;
				++dest;
			}
			else if (isNumeric(*src))
			{
				*dest = *src;
				++dest;
			}
			else
			{
				*dest = '_';
				++dest;
			}
			++src;
		}
		*dest = 0;
	}

	StudioAngelScriptPlugin(StudioApp& app, const char* name)
		: m_app(app)
		, m_name(name, app.getAllocator())
		, m_script_module(nullptr)
		, m_script_context(nullptr)
	{
	}

	~StudioAngelScriptPlugin()
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

	bool exportData(const char* dest_dir) override
	{
		// AngelScript doesn't need additional DLLs to export
		return true;
	}

	void onGUI() override
	{
		if (!m_script_module || !m_script_context) return;

		// Check window action
		if (m_action.get() && m_app.checkShortcut(*m_action.get(), true))
		{
			asIScriptFunction* window_action_func = m_script_module->GetFunctionByDecl("void windowMenuAction()");
			if (window_action_func)
			{
				m_script_context->Prepare(window_action_func);
				m_script_context->Execute();
			}
		}

		// Call GUI function
		asIScriptFunction* gui_func = m_script_module->GetFunctionByDecl("void gui()");
		if (gui_func)
		{
			m_script_context->Prepare(gui_func);
			m_script_context->Execute();
		}
	}

	void onSettingsLoaded() override
	{
		if (!m_script_module || !m_script_context) return;

		asIScriptFunction* settings_func = m_script_module->GetFunctionByDecl("void onSettingsLoaded()");
		if (settings_func)
		{
			m_script_context->Prepare(settings_func);
			m_script_context->Execute();
		}
	}

	void onBeforeSettingsSaved() override
	{
		if (!m_script_module || !m_script_context) return;

		asIScriptFunction* settings_func = m_script_module->GetFunctionByDecl("void onBeforeSettingsSaved()");
		if (settings_func)
		{
			m_script_context->Prepare(settings_func);
			m_script_context->Execute();
		}
	}

	const char* getName() const override { return m_name.c_str(); }

	StudioApp& m_app;
	Path m_path;
	Local<Action> m_action;
	String m_name;
	asIScriptModule* m_script_module;
	asIScriptContext* m_script_context;
};

struct EditorWindow : AssetEditorWindow
{
	EditorWindow(const Path& path, StudioApp& app)
		: AssetEditorWindow(app)
		, m_app(app)
		, m_path(path)
	{
		m_file_async_handle =
			app.getEngine().getFileSystem().getContent(path, makeDelegate<&EditorWindow::onFileLoaded>(this));
	}

	~EditorWindow()
	{
		if (m_file_async_handle.isValid())
		{
			m_app.getEngine().getFileSystem().cancel(m_file_async_handle);
		}
	}

	void onFileLoaded(Span<const u8> data, bool success)
	{
		m_file_async_handle = FileSystem::AsyncHandle::invalid();
		if (success)
		{
			StringView v;
			v.begin = (const char*)data.begin();
			v.end = (const char*)data.end();
			m_code_editor = createAngelScriptCodeEditor(m_app);
			m_code_editor->setText(v);
		}
	}

	void save()
	{
		OutputMemoryStream blob(m_app.getAllocator());
		m_code_editor->serializeText(blob);
		m_app.getAssetBrowser().saveResource(m_path, blob);
		m_dirty = false;
	}

	void windowGUI() override
	{
		CommonActions& actions = m_app.getCommonActions();

		if (ImGui::BeginMenuBar())
		{
			if (actions.save.iconButton(m_dirty, &m_app)) save();
			if (actions.open_externally.iconButton(true, &m_app)) m_app.getAssetBrowser().openInExternalEditor(m_path);
			if (actions.view_in_browser.iconButton(true, &m_app)) m_app.getAssetBrowser().locate(m_path);
			ImGui::EndMenuBar();
		}

		if (m_file_async_handle.isValid())
		{
			ImGui::TextUnformatted("Loading...");
			return;
		}

		if (m_code_editor)
		{
			ImGui::PushFont(m_app.getMonospaceFont());

			if (m_code_editor->gui("codeeditor", ImVec2(0, 0), m_app.getDefaultFont()))
			{
				m_dirty = true;
			}

			ImGui::PopFont();
		}
	}

	const Path& getPath() override { return m_path; }
	const char* getName() const override { return "angelscript editor"; }

private:
	UniquePtr<CodeEditor> createAngelScriptCodeEditor(StudioApp& app)
	{
		// Create a basic code editor for AngelScript
		// This would need to be implemented based on your code editor framework
		return createLuaCodeEditor(app); // Reuse Lua editor for now
	}

	StudioApp& m_app;
	FileSystem::AsyncHandle m_file_async_handle = FileSystem::AsyncHandle::invalid();
	Path m_path;
	UniquePtr<CodeEditor> m_code_editor;
};

static bool gatherIncludes(Span<const u8> src, Lumix::Array<Path>& dependencies, const Path& path)
{
	// Simple include parsing for AngelScript
	const char* content = (const char*)src.begin();
	const char* end = (const char*)src.end();

	while (content < end)
	{
		// Look for #include directives
		if (strncmp(content, "#include", 8) == 0)
		{
			content += 8;
			// Skip whitespace
			while (content < end && (*content == ' ' || *content == '\t')) content++;

			if (content < end && *content == '"')
			{
				content++; // Skip opening quote
				const char* start = content;
				while (content < end && *content != '"') content++;
				if (content < end)
				{
					StringView include_path(start, u32(content - start));
					dependencies.push(Path(include_path));
				}
			}
		}

		// Move to next line
		while (content < end && *content != '\n') content++;
		if (content < end) content++; // Skip newline
	}

	return true;
}

struct AssetPlugin : AssetBrowser::IPlugin, AssetCompiler::IPlugin
{
	explicit AssetPlugin(StudioApp& app)
		: m_app(app)
	{
		app.getAssetCompiler().registerExtension("as", ASScript::TYPE);
	}

	void openEditor(const Path& path) override
	{
		IAllocator& allocator = m_app.getAllocator();
		UniquePtr<EditorWindow> win = UniquePtr<EditorWindow>::create(allocator, path, m_app);
		m_app.getAssetBrowser().addWindow(win.move());
	}

	bool compile(const Path& src) override
	{
		FileSystem& fs = m_app.getEngine().getFileSystem();
		OutputMemoryStream src_data(m_app.getAllocator());
		if (!fs.getContentSync(src, src_data)) return false;

		Array<Path> deps(m_app.getAllocator());
		if (!gatherIncludes(src_data, deps, src)) return false;

		OutputMemoryStream out(m_app.getAllocator());
		out.write(deps.size());
		for (const Path& dep : deps)
		{
			out.writeString(dep.c_str());
		}
		out.write(src_data.data(), src_data.size());
		return m_app.getAssetCompiler().writeCompiledResource(src, out);
	}

	const char* getLabel() const override { return "AngelScript"; }
	ResourceType getResourceType() const override { return ASScript::TYPE; }
	bool canCreateResource() const override { return true; }
	const char* getDefaultExtension() const override { return "as"; }

	void createResource(OutputMemoryStream& blob) override { blob << "void update(float time_delta)\n{\n}\n"; }

	StudioApp& m_app;
};

struct AddComponentPlugin final : StudioApp::IAddComponentPlugin
{
	explicit AddComponentPlugin(StudioApp& app)
		: app(app)
		, file_selector("as", app)
	{
	}

	void onGUI(bool create_entity, bool, EntityPtr parent, WorldEditor& editor) override
	{
		if (!ImGui::BeginMenu("File")) return;
		Path path;
		AssetBrowser& asset_browser = app.getAssetBrowser();
		bool new_created = false;
		if (ImGui::BeginMenu("New"))
		{
			file_selector.gui(false, "as");
			if (ImGui::Button("Create"))
			{
				path = file_selector.getPath();
				os::OutputFile file;
				FileSystem& fs = app.getEngine().getFileSystem();
				if (fs.open(file_selector.getPath(), file))
				{
					new_created = true;
					file.close();
				}
				else
				{
					logError("Failed to create ", path);
				}
			}
			ImGui::EndMenu();
		}
		bool create_empty = ImGui::Selectable("Empty", false);

		static FilePathHash selected_res_hash;
		if (asset_browser.resourceList(path, selected_res_hash, ASScript::TYPE, false) || create_empty || new_created)
		{
			editor.beginCommandGroup("createEntityWithComponent");
			if (create_entity)
			{
				EntityRef entity = editor.addEntity();
				editor.selectEntities(Span(&entity, 1), false);
			}
			if (editor.getSelectedEntities().empty()) return;
			EntityRef entity = editor.getSelectedEntities()[0];

			if (!editor.getWorld()->hasComponent(entity, ANGELSCRIPT_TYPE))
			{
				editor.addComponent(Span(&entity, 1), ANGELSCRIPT_TYPE);
			}

			const ComponentUID cmp(entity, ANGELSCRIPT_TYPE, editor.getWorld()->getModule(ANGELSCRIPT_TYPE));
			editor.addArrayPropertyItem(cmp, "scripts");

			if (!create_empty)
			{
				auto* script_scene = static_cast<AngelScriptModule*>(editor.getWorld()->getModule(ANGELSCRIPT_TYPE));
				int scr_count = script_scene->getScriptCount(entity);
				editor.setProperty(
					cmp.type, "scripts", scr_count - 1, "Path", Span((const EntityRef*)&entity, 1), path);
			}
			if (parent.isValid()) editor.makeParent(parent, entity);
			editor.endCommandGroup();
			editor.lockGroupCommand();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndMenu();
	}

	const char* getLabel() const override { return "AngelScript / File"; }

	StudioApp& app;
	FileSelector file_selector;
};

struct PropertyGridPlugin final : PropertyGrid::IPlugin
{
	void onGUI(PropertyGrid& grid,
		Span<const EntityRef> entities,
		ComponentType cmp_type,
		const TextFilter& filter,
		WorldEditor& editor) override
	{
		if (filter.isActive()) return;
		if (cmp_type != ANGELSCRIPT_TYPE) return;
		if (entities.length() != 1) return;

		AngelScriptModule* module = (AngelScriptModule*)editor.getWorld()->getModule(cmp_type);
		const EntityRef e = entities[0];
		const u32 count = module->getScriptCount(e);
		for (u32 i = 0; i < count; ++i)
		{
			if (module->beginFunctionCall(e, i, "onGUI"))
			{
				module->endFunctionCall();
			}
		}
	}
};

struct AngelScriptAction
{
	void run()
	{
		if (!script_module || !script_context) return;

		asIScriptFunction* run_func = script_module->GetFunctionByDecl("void run()");
		if (run_func)
		{
			script_context->Prepare(run_func);
			script_context->Execute();
		}
	}

	Local<Action> action;
	asIScriptModule* script_module;
	asIScriptContext* script_context;
};

struct StudioAppPlugin : StudioApp::IPlugin
{
	StudioAppPlugin(StudioApp& app)
		: m_app(app)
		, m_asset_plugin(app)
		, m_angelscript_actions(app.getAllocator())
		, m_plugins(app.getAllocator())
	{
		AngelScriptSystem* system = (AngelScriptSystem*)app.getEngine().getSystemManager().getSystem("angelscript");
		asIScriptEngine* engine = system->getEngine();

		// Register editor API functions for AngelScript
		registerEditorAPI(engine);

		initPlugins();
	}

	void update(float) override
	{
		for (AngelScriptAction* action : m_angelscript_actions)
		{
			if (m_app.checkShortcut(*action->action, true)) action->run();
		}
	}

	void registerEditorAPI(asIScriptEngine* engine)
	{
		int r;

		// Register basic editor functions
		r = engine->RegisterGlobalFunction(
			"void createEntity()", asMETHOD(StudioAppPlugin, createEntity), asCALL_THISCALL_ASGLOBAL, this);
		ASSERT(r >= 0);

		r = engine->RegisterGlobalFunction("int getSelectedEntitiesCount()",
			asMETHOD(StudioAppPlugin, getSelectedEntitiesCount),
			asCALL_THISCALL_ASGLOBAL,
			this);
		ASSERT(r >= 0);

		r = engine->RegisterGlobalFunction("Entity getSelectedEntity(uint)",
			asMETHOD(StudioAppPlugin, getSelectedEntity),
			asCALL_THISCALL_ASGLOBAL,
			this);
		ASSERT(r >= 0);

		r = engine->RegisterGlobalFunction("void createComponent(Entity, const String &in)",
			asMETHOD(StudioAppPlugin, createComponent),
			asCALL_THISCALL_ASGLOBAL,
			this);
		ASSERT(r >= 0);
	}

	void initPlugins()
	{
		FileSystem& fs = m_app.getEngine().getFileSystem();
		os::FileIterator* iter = fs.createFileIterator("editor/scripts/plugins");
		os::FileInfo info;
		while (os::getNextFile(iter, &info))
		{
			if (info.is_directory) continue;
			if (!Path::hasExtension(info.filename, "as")) continue;

			OutputMemoryStream blob(m_app.getAllocator());
			const Path path("editor/scripts/plugins/", info.filename);
			if (!fs.getContentSync(path, blob)) continue;

			StringView content;
			content.begin = (const char*)blob.data();
			content.end = content.begin + blob.size();
			StudioAngelScriptPlugin* plugin = StudioAngelScriptPlugin::create(m_app, content, path);
			if (plugin) m_plugins.push(plugin);
		}
		os::destroyFileIterator(iter);
	}

	const char* getName() const override { return "angelscript"; }

	// Editor API implementations
	EntityRef createEntity() { return m_app.getWorldEditor().addEntity(); }
	i32 getSelectedEntitiesCount() { return m_app.getWorldEditor().getSelectedEntities().size(); }
	EntityRef getSelectedEntity(u32 idx) { return m_app.getWorldEditor().getSelectedEntities()[idx]; }
	void createComponent(EntityRef e, const String& type)
	{
		const ComponentType cmp_type = reflection::getComponentType(type.c_str());
		m_app.getWorldEditor().addComponent(Span(&e, 1), cmp_type);
	}

	void checkScriptCommandLine()
	{
		char command_line[1024];
		os::getCommandLine(Span(command_line));
		CommandLineParser parser(command_line);
		while (parser.next())
		{
			if (parser.currentEquals("-run_angelscript"))
			{
				if (!parser.next()) break;

				char tmp[MAX_PATH];
				parser.getCurrent(tmp, lengthOf(tmp));
				OutputMemoryStream content(m_app.getAllocator());

				if (m_app.getEngine().getFileSystem().getContentSync(Path(tmp), content))
				{
					content.write('\0');
					runScript((const char*)content.data(), tmp);
				}
				else
				{
					logError("Could not read ", tmp);
				}
				break;
			}
		}
	}

	void init() override
	{
		AddComponentPlugin* add_cmp_plugin = LUMIX_NEW(m_app.getAllocator(), AddComponentPlugin)(m_app);
		m_app.registerComponent(ICON_FA_SCROLL, "angelscript", *add_cmp_plugin);

		const char* exts[] = {"as"};
		m_app.getAssetCompiler().addPlugin(m_asset_plugin, Span(exts));
		m_app.getAssetBrowser().addPlugin(m_asset_plugin, Span(exts));
		m_app.getPropertyGrid().addPlugin(m_property_grid_plugin);

		checkScriptCommandLine();
	}

	void runScript(const char* src, const char* script_name)
	{
		AngelScriptSystem* system = (AngelScriptSystem*)m_app.getEngine().getSystemManager().getSystem("angelscript");
		asIScriptEngine* engine = system->getEngine();

		asIScriptModule* module = engine->GetModule("TempScript", asGM_CREATE_IF_NOT_EXISTS);
		int r = module->AddScriptSection(script_name, src);
		if (r < 0)
		{
			logError(script_name, ": failed to add script section");
			return;
		}

		r = module->Build();
		if (r < 0)
		{
			logError(script_name, ": failed to build script");
			return;
		}

		asIScriptFunction* main_func = module->GetFunctionByDecl("void main()");
		if (main_func)
		{
			asIScriptContext* ctx = engine->CreateContext();
			ctx->Prepare(main_func);
			r = ctx->Execute();
			if (r != asEXECUTION_FINISHED)
			{
				logError(script_name, ": script execution failed");
			}
			ctx->Release();
		}
	}

	~StudioAppPlugin()
	{
		m_app.getAssetCompiler().removePlugin(m_asset_plugin);
		m_app.getAssetBrowser().removePlugin(m_asset_plugin);
		m_app.getPropertyGrid().removePlugin(m_property_grid_plugin);

		for (StudioAngelScriptPlugin* plugin : m_plugins)
		{
			m_app.removePlugin(*plugin);
			LUMIX_DELETE(m_app.getAllocator(), plugin);
		}

		for (AngelScriptAction* action : m_angelscript_actions)
		{
			LUMIX_DELETE(m_app.getAllocator(), action);
		}
	}

	bool showGizmo(WorldView& view, ComponentUID cmp) override
	{
		if (cmp.type == ANGELSCRIPT_TYPE)
		{
			auto* module = static_cast<AngelScriptModule*>(cmp.module);
			int count = module->getScriptCount((EntityRef)cmp.entity);
			for (int i = 0; i < count; ++i)
			{
				if (module->beginFunctionCall((EntityRef)cmp.entity, i, "onDrawGizmo"))
				{
					module->endFunctionCall();
				}
			}
			return true;
		}
		return false;
	}

	StudioApp& m_app;
	AssetPlugin m_asset_plugin;
	PropertyGridPlugin m_property_grid_plugin;
	Array<AngelScriptAction*> m_angelscript_actions;
	Array<StudioAngelScriptPlugin*> m_plugins;
};

} // anonymous namespace

LUMIX_STUDIO_ENTRY(angelscript)
{
	PROFILE_FUNCTION();
	IAllocator& allocator = app.getAllocator();
	return LUMIX_NEW(allocator, StudioAppPlugin)(app);
}