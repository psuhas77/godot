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
#include "core/os/file_access.h"
#include "editor/editor_node.h"
#include "modules/gdscript/gdscript_parser.h"
#include "scene/resources/resource_format_text.h"

void StaticAnalyzerDialog::show() {
	scenes->clear();
	scripts->clear();
	TreeItem *root = scenes->create_item();
	TreeItem *rootscript = scripts->create_item();
	_traverse_scenes(EditorFileSystem::get_singleton()->get_filesystem(), root, rootscript);
	popup_centered_ratio();
}

void StaticAnalyzerDialog::_traverse_scenes(EditorFileSystemDirectory *efsd, TreeItem *root, TreeItem *rootscript) {

	if (!efsd)
		return;

	for (int i = 0; i < efsd->get_subdir_count(); i++) {

		_traverse_scenes(efsd->get_subdir(i), root, rootscript);
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

			if (!scene_broken) {
				Ref<PackedScene> ps = rilt->get_resource();
				Ref<SceneState> ss = ps->get_state(); //contains the scene tree

				for (int j = 0; j < ss->get_node_count(); j++) {

					if (ss->get_node_instance(j).is_valid()) {
						Ref<Script> scr = ss->get_node_instance(j)->instance()->get_script();
						if (!scr.is_null()) {
							_traverse_script(scr->get_source_code(), scr->get_path());
						}
					}

					for (int k = 0; k < ss->get_node_property_count(j); k++) {
						String node_prop = ss->get_node_property_name(j, k);

						if (node_prop == "script") {
							Ref<Script> scr = ss->get_node_property_value(j, k);
							_traverse_script(scr->get_source_code(), scr->get_path());
						}

						//Add checks for other resources
					}
				}
			}
		}
	}
}

void StaticAnalyzerDialog::_traverse_script(String &p_code, String &p_self_path) {

	GDScriptParser parser;
	parser.clear();
	Error e = parser.parse(p_code, "", false, p_self_path);
	const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(parser.get_parse_tree());

	check_variables(c);

	for (int i = 0; i < c->functions.size(); i++) {
		check_function(c->functions[i]);
	}

	
}

void StaticAnalyzerDialog::check_variables(const GDScriptParser::Node *n) {

	if (n->type == n->TYPE_CLASS) {

		const GDScriptParser::ClassNode *c = static_cast<const GDScriptParser::ClassNode *>(n);

		for (int i = 0; i < c->variables.size(); i++) {

			if (c->variables[i].expression->type == c->TYPE_OPERATOR) {

				GDScriptParser::OperatorNode *o = static_cast<GDScriptParser::OperatorNode *>(c->variables[i].expression);

				for (int j = 0; j < o->arguments.size(); j++) {

					if (o->arguments[j]->type == o->TYPE_IDENTIFIER) {

						GDScriptParser::IdentifierNode *idn = static_cast<GDScriptParser::IdentifierNode *>(o->arguments[j]);

						if (idn->name == "get_node") {
							//check get_node call
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
				}
			}
			
			if (o->arguments[j]->type == o->TYPE_OPERATOR) {
				check_variables(o->arguments[j]);
			}
		}
	}
}

void StaticAnalyzerDialog::check_function(const GDScriptParser::Node *n) {

	if (n->type == n->TYPE_FUNCTION) {

		const GDScriptParser::FunctionNode *f = static_cast<const GDScriptParser::FunctionNode *>(n);

		for (int i = 0; i < f->body->statements.size(); i++) {

			check_function(f->body->statements[i]);
		}
	}

	if (n->type == n->TYPE_OPERATOR) {

		const GDScriptParser::OperatorNode *o = static_cast<const GDScriptParser::OperatorNode *>(n);

		for (int i = 0; i < o->arguments.size(); i++) {

			if (o->arguments[i]->type == o->TYPE_IDENTIFIER) {

				GDScriptParser::IdentifierNode *idn = static_cast<GDScriptParser::IdentifierNode *>(o->arguments[i]);

				if (idn->name == "get_node") {
					//check get_node call
				}
			}
			if (o->arguments[i]->type == o->TYPE_OPERATOR) {
				check_function(o->arguments[i]);
			}
		}
	}

	if (n->type == n->TYPE_CONTROL_FLOW) {

		const GDScriptParser::ControlFlowNode *cf = static_cast<const GDScriptParser::ControlFlowNode *>(n);

		if (cf->body) {

			for (int i = 0; i < cf->body->statements.size(); i++) {

				check_function(cf->body->statements[i]);
			}
		}
		if (cf->body_else) {

			for (int i = 0; i < cf->body_else->statements.size(); i++) {

				check_function(cf->body_else->statements[i]);
			}
		}
	}

	if (n->type == n->TYPE_BLOCK) {

		const GDScriptParser::BlockNode *b = static_cast<const GDScriptParser::BlockNode *>(n);

		if (!b->statements.empty()) {
			for (int i = 0; i < b->statements.size(); i++) {
				check_function(b->statements[i]);
			}
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
	scripts->set_column_title(0, TTR("Node"));
	scripts->set_column_title(1, TTR("Script"));
	scripts->set_column_title(2, TTR("Issue"));
	scripts->set_column_titles_visible(true);
	scripts->set_hide_root(true);
	vb->add_margin_child("scripts", scripts, true);
}
