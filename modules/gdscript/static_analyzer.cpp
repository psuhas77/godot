/*************************************************************************/
/*  static_analyzer.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "static_analyzer.h"

#include "core/io/resource_loader.h"
#include "core/object.h"
#include "core/os/file_access.h"
#include "editor/editor_node.h"
#include "modules/gdscript/gdscript_parser.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/resource_format_text.h"

void StaticAnalyzerDialog::show() {
	scenes->clear();
	scripts->clear();
	scene_cache.clear();
	script_error_cache.clear();
	script_traversed.clear();
	root = scenes->create_item();
	rootscript = scripts->create_item();

	EditorFileSystemDirectory *efsd = EditorFileSystem::get_singleton()->get_filesystem();
	_traverse_scenes(efsd, root, rootscript, true); // building the scence cache
	_traverse_scenes(efsd, root, rootscript, false);
	popup_centered_ratio();
}

void StaticAnalyzerDialog::_traverse_scenes(EditorFileSystemDirectory *efsd, TreeItem *root, TreeItem *rootscript, bool pre_instance) {

	if (!efsd)
		return;

	for (int i = 0; i < efsd->get_subdir_count(); i++) {

		_traverse_scenes(efsd->get_subdir(i), root, rootscript, pre_instance);
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {

		if (ClassDB::is_parent_class(efsd->get_file_type(i), "PackedScene")) {

			Ref<ResourceInteractiveLoaderText> rilt = ResourceFormatLoaderText().load_interactive(efsd->get_file_path(i));
			rilt->set_continue_check(true);
			bool scene_broken = false;

			while (true) {
				Error err = rilt->poll();
				if (err == ERR_FILE_EOF) {
					break;
				}
				if (err == ERR_FILE_MISSING_DEPENDENCIES) {
					//there is a missing external resource
					TreeItem *scene_item = scenes->create_item(root);
					scene_item->set_text(0, efsd->get_file_path(i));
					scene_item->set_text(1, "line " + rilt->get_lines() + ": " + rilt->get_error_text());
				}
				if (err == ERR_FILE_CORRUPT) {
					//TSCN file is broken
					TreeItem *scene_item = scenes->create_item(root);
					scene_item->set_text(0, efsd->get_file_path(i));
					scene_item->set_text(1, "line " + rilt->get_lines() + ": " + rilt->get_error_text());
					scene_broken = true;
					break;
				}
				if (err == ERR_PARSE_ERROR) {
					//TSCN file is broken
					TreeItem *scene_item = scenes->create_item(root);
					scene_item->set_text(0, efsd->get_file_path(i));
					scene_item->set_text(1, "line " + rilt->get_lines() + ": " + rilt->get_error_text());
					scene_broken = true;
					break;
				}
			}

			if (!scene_broken && pre_instance) {
				//Ref<PackedScene> ps = rilt->get_resource();
				Ref<PackedScene> ps = Ref<PackedScene>(Object::cast_to<PackedScene>(*rilt->get_resource()));

				if (ps->can_instance()) {
					ps->instance(ps->GEN_EDIT_STATE_MAIN);
				}

				String scene_path = efsd->get_file_path(i);

				String scene_name = scene_path.substr(6, scene_path.length() - 11);
				scene_cache[scene_name] = ps;

			}

			else if (!scene_broken && !pre_instance) {
				Ref<PackedScene> ps = Ref<PackedScene>(Object::cast_to<PackedScene>(*rilt->get_resource()));

				if (ps->get_path().empty()) {
					ps->set_path(efsd->get_file_path(i), true);
				}

				if (ps->get_name().empty()) {
					ps->set_name(efsd->get_file_path(i).substr(6, efsd->get_file_path(i).length() - 11));
				}

				current_scene = ps;

				Ref<SceneState> ss = ps->get_state(); //contains the scene tree

				for (int j = 0; j < ss->get_node_count(); j++) {

					if (ss->get_node_instance(j).is_valid()) {
						Ref<Script> scr = ss->get_node_instance(j)->instance()->get_script();
						current_script = scr;
						if (!scr.is_null()) {
							current_node_id = j;
							_traverse_script(scr->get_source_code(), scr->get_path());
						}
					}

					for (int k = 0; k < ss->get_node_property_count(j); k++) {
						String node_prop = ss->get_node_property_name(j, k);

						if (node_prop == "script") {
							Ref<Script> scr = ss->get_node_property_value(j, k);

							if (scr.is_valid()) {

								current_script = scr;
								current_node_id = j;
								_traverse_script(scr->get_source_code(), scr->get_path());
							}
						}

						//Add checks for other resources
					}
				}
			}
		}
	}
}

void StaticAnalyzerDialog::_traverse_script(const String &p_code, const String &p_self_path) {

	GDScriptParser parser;
	parser.clear();
	Error e = parser.parse(p_code, "", false, p_self_path);
	const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(parser.get_parse_tree()); //main class
	current_class = c;

	String script_name = reduce_script_name(current_script->get_path());

	bool has_scene = false;
	String class_type;

	if (scene_cache[script_name] != NULL) {

		class_type = scene_cache[script_name]->get_state()->get_node_type(0);
	}

	//check to ensure script inherits the object type it is attached to
	if (has_scene && c->extends_class[0] != class_type) {

		String context = "";
		String issue = "Script " + current_script->get_path() + " does not inherit the object type it's attached to";

		const script_errors scr_err = script_errors(script_name, context, issue);

		add_script_error(scr_err);
	}

	check_class(current_class);

	script_traversed.push_back(script_name);

}

void StaticAnalyzerDialog::add_script_error(const script_errors scr_err) {

	if (script_error_cache.find(scr_err) == -1) {

		TreeItem *script_item = scripts->create_item(rootscript);

		script_item->set_text(0, scr_err.script);
		script_item->set_text(1, scr_err.context);
		script_item->set_text(2, scr_err.issue);

		script_error_cache.push_back(scr_err);
	}
}

void StaticAnalyzerDialog::check_class(const GDScriptParser::Node *n) {

	const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(n);
	check_variables(c);

	for (int i = 0; i < c->functions.size(); i++) {

		current_function = c->functions[i];
		check_function(c->functions[i]);
	}
	current_function = NULL;

	for (int i = 0; i < c->subclasses.size(); i++) {

		current_class = c->subclasses[i];
		check_class(current_class);
	}
}

void StaticAnalyzerDialog::check_variables(const GDScriptParser::Node *n) {

	if (n->type == n->TYPE_CLASS) {

		const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(n);

		for (int i = 0; i < c->variables.size(); i++) {

			if (c->variables[i].expression && c->variables[i].expression->type == c->TYPE_OPERATOR) {

				GDScriptParser::OperatorNode *o = static_cast<GDScriptParser::OperatorNode *>(c->variables[i].expression);

				for (int j = 0; j < o->arguments.size(); j++) {

					if (o->arguments[j]->type == o->TYPE_IDENTIFIER) {

						GDScriptParser::IdentifierNode *idn = static_cast<GDScriptParser::IdentifierNode *>(o->arguments[j]);

						if (idn->name == "get_node") {
							//check get_node call
							check_node_path(o->arguments[j + 1]);
						}
					}
					if (o->arguments[j]->type == o->TYPE_OPERATOR) {
						check_variables(o->arguments[j]);
					}
				}
			}
		}
	}

	if (n->type == n->TYPE_OPERATOR) {

		const GDScriptParser::OperatorNode *o = static_cast<const GDScriptParser::OperatorNode *>(n);

		for (int j = 0; j < o->arguments.size(); j++) {

			if (o->arguments[j]->type == o->TYPE_IDENTIFIER) {

				GDScriptParser::IdentifierNode *idn = static_cast<GDScriptParser::IdentifierNode *>(o->arguments[j]);

				if (idn->name == "get_node") {
					//check get_node call
					check_node_path(o->arguments[j + 1]);
				}
			}

			if (o->arguments[j]->type == o->TYPE_OPERATOR) {
				check_variables(o->arguments[j]);
			}
		}
	}
}

String StaticAnalyzerDialog::reduce_script_name(String scr_name) {

	return scr_name.substr(6, current_script->get_path().length() - 9);
}

void StaticAnalyzerDialog::check_function(const GDScriptParser::FunctionNode *n) {

	const GDScriptParser::FunctionNode *f = static_cast<const GDScriptParser::FunctionNode *>(n);

	String script_name = reduce_script_name(current_script->get_path());

	bool has_node = false;

	if (scene_cache[script_name] != NULL) {
		has_node = true;
	}

	//check to ensure if the function is connected to a signal, then it has correct arity and type
	if (has_node && script_traversed.find(script_name) == -1) {

		Ref<SceneState> ss = scene_cache[script_name]->get_state();

		for (int i = 0; i < ss->get_connection_count(); i++) {
			if (ss->get_connection_method(i) == f->name) {
				MethodInfo sig;

				String class_type = ss->get_connection_source(i);

				if (class_type == ".") {
					class_type = scene_cache[script_name]->get_state()->get_node_type(0);
				}
				
				if (ClassDB::get_signal(class_type, ss->get_connection_signal(i), &sig)) {
					if (sig.arguments.size() != f->arguments.size()) {

						String context = "";
						String issue = "line: " + itos(f->line) + "  Function " + String(f->name).quote() + " connected to signal " + String(ss->get_connection_signal(i)).quote() + " has wrong arity";

						const script_errors scr_err = script_errors(script_name,context,issue);

						add_script_error(scr_err);
					}

				}

			}
		}

	}

	for (int i = 0; i < f->body->statements.size(); i++) {

		check_statement(f->body->statements[i]);
	}
}

void StaticAnalyzerDialog::check_statement(const GDScriptParser::Node *n) {

	if (n->type == n->TYPE_OPERATOR) {

		const GDScriptParser::OperatorNode *o = static_cast<const GDScriptParser::OperatorNode *>(n);

		for (int i = 0; i < o->arguments.size(); i++) {

			if (o->arguments[i]->type == o->TYPE_IDENTIFIER) {

				GDScriptParser::IdentifierNode *idn = static_cast<GDScriptParser::IdentifierNode *>(o->arguments[i]);

				if (idn->name == "get_node") {
					//check get_node call
					check_node_path(o->arguments[i + 1]);
				}
			}

			if (o->arguments[i]->type == o->TYPE_OPERATOR) {
				check_statement(o->arguments[i]);
			}
		}
	}

	if (n->type == n->TYPE_CONTROL_FLOW) {

		const GDScriptParser::ControlFlowNode *cf = static_cast<const GDScriptParser::ControlFlowNode *>(n);

		if (cf->body) {

			for (int i = 0; i < cf->body->statements.size(); i++) {

				check_statement(cf->body->statements[i]);
			}
		}
		if (cf->body_else) {

			for (int i = 0; i < cf->body_else->statements.size(); i++) {

				check_statement(cf->body_else->statements[i]);
			}
		}
	}

	if (n->type == n->TYPE_BLOCK) {

		const GDScriptParser::BlockNode *b = static_cast<const GDScriptParser::BlockNode *>(n);

		if (!b->statements.empty()) {

			for (int i = 0; i < b->statements.size(); i++) {
				check_statement(b->statements[i]);
			}
		}
	}
}

void StaticAnalyzerDialog::check_node_path(const GDScriptParser::Node *n) {

	const GDScriptParser::ConstantNode *cn;
	String path;
	if (n->type == n->TYPE_CONSTANT) {

		cn = static_cast<const GDScriptParser::ConstantNode *>(n);

		path = (String)cn->value;

		NodePath node_path = NodePath(path);

		String script_name = reduce_script_name(current_script->get_path());

		bool script_has_node = false;

		if (scene_cache[script_name] != NULL) {
			script_has_node = true;
		}

		bool node_found = false;
		String first_path = node_path.get_name(0);

		if (first_path == "root") {

			first_path = node_path.get_name(1);
			Vector<StringName> mod_path;

			for (int i = 2; i < node_path.get_name_count(); i++) {

				mod_path.push_back(node_path.get_name(i));
			}

			if (mod_path.empty()) {
				mod_path.push_back(".");
			}

			node_path = NodePath(mod_path, false);
		}

		if (script_has_node && !scene_cache[script_name].is_null() && scene_cache[script_name]->get_state()->find_node_by_path(node_path) != -1) {

			node_found = true;
		}

		else if (!scene_cache[first_path].is_null() && scene_cache[first_path]->get_state()->find_node_by_path(node_path) != -1) {

			node_found = true;
		}

		String context = current_scene->get_name() + current_scene->get_state()->get_node_path(current_node_id, true);
		String issue = "line " + itos(cn->line) + " : node in " + path.quote() + " can't be found";

		const script_errors scr_err = script_errors(script_name, context, issue);

		if (!node_found) {
			add_script_error(scr_err);
		}
	}

	//if get_node call is through an identifier
	if (n->type == n->TYPE_IDENTIFIER) {
		const GDScriptParser::IdentifierNode *in = static_cast<const GDScriptParser::IdentifierNode *>(n);

		if (!current_function) {

			for (int i = 0; i < current_class->variables.size(); i++) {

				if (current_class->variables[i].identifier == in->name && current_class->variables[i].expression) {
					check_node_path(current_class->variables[i].expression);
					break;
				}
			}
		}

		else if (current_function && current_function->body->variables.has(in->name)) {

			check_node_path(current_function->body->variables[in->name]->assign);
		}
	}
}

StaticAnalyzerDialog::StaticAnalyzerDialog() {

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	scenes = memnew(Tree);
	scenes->set_columns(2);
	scenes->set_column_title(0, TTR("Scenes"));
	scenes->set_column_title(1, TTR("Issue"));
	scenes->set_column_titles_visible(true);
	scenes->set_hide_root(true);
	set_title(TTR("Static Analyzer"));
	vb->add_margin_child("scenes", scenes, true);
	scripts = memnew(Tree);
	scripts->set_columns(3);
	scripts->set_column_title(0, TTR("Script"));
	scripts->set_column_title(1, TTR("Context"));
	scripts->set_column_title(2, TTR("Issue"));
	scripts->set_column_titles_visible(true);
	scripts->set_hide_root(true);
	vb->add_margin_child("scripts", scripts, true);
}
