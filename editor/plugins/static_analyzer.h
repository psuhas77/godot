/*************************************************************************/
/*  static_analyzer.h                                                */
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

#ifndef STATIC_ANALYZER_H
#define STATIC_ANALYZER_H

#include "editor_file_dialog.h"
#include "editor_file_system.h"
#include "modules/gdscript/gdscript_parser.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"

class StaticAnalyzerDialog : public ConfirmationDialog {
    GDCLASS(StaticAnalyzerDialog, ConfirmationDialog);

	HashMap<String,Ref<PackedScene>> scene_cache; 
    Tree *scenes;
	Tree *scripts;
	TreeItem *root;
	TreeItem *rootscript;
	Ref<Script> current_script;
	Ref<PackedScene> current_scene;
	int current_node_id;

	struct script_errors {
		String script;
		String context;
		String issue;

		script_errors() {}

		script_errors(String scr, String con, String iss) {
			script = scr;
			context = con;
			issue = iss;
		}

		friend bool operator==(const script_errors& l, const script_errors& r) {
			 if (l.script == r.script && l.context == r.context && l.issue == r.issue){return true;}
			 return false;
		}

	};

	Vector<script_errors> script_error_cache;

	const GDScriptParser::ClassNode *current_class;
	GDScriptParser::FunctionNode *current_function;
	void _traverse_scenes(EditorFileSystemDirectory *efsd, TreeItem *root, TreeItem *rootscript, bool pre_instance = false);
	void _traverse_script(const String &p_code, const String &p_self_path);

	void check_class(const GDScriptParser::Node *n);
	void check_function(const GDScriptParser::Node *n);
	void check_node_path(const GDScriptParser::Node *n);
	void check_variables(const GDScriptParser::Node *n);
    
public:
    void show();
    StaticAnalyzerDialog();
};

#endif // STATIC_ANALYZER_H

