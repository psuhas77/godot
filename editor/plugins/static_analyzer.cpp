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

void StaticAnalyzerDialog::show() {
    scenes->clear();
    TreeItem* root = scenes->create_item();
    _traverse_scenes(EditorFileSystem::get_singleton()->get_filesystem(),root);
	popup_centered_ratio();
}

void StaticAnalyzerDialog::_traverse_scenes(EditorFileSystemDirectory *efsd,TreeItem *root){
    
    if (!efsd)
		return;
    
    for (int i = 0; i < efsd->get_subdir_count(); i++) {
        
        _traverse_scenes(efsd->get_subdir(i),root);
    }
    
    for (int i = 0; i < efsd->get_file_count(); i++) {
        
        if (ClassDB::is_parent_class(efsd->get_file_type(i), "PackedScene")){
        
            TreeItem* scene_item = scenes->create_item(root);
            scene_item->set_text(0, efsd->get_file_path(i));
        }
        
    }
}

StaticAnalyzerDialog::StaticAnalyzerDialog(){
    
    VBoxContainer* vb = memnew(VBoxContainer);
    add_child(vb);
    
    scenes= memnew(Tree);
    scenes->set_columns(1);
    scenes->set_column_title(0, TTR("Scenes"));
    scenes->set_hide_root(true);
    set_title(TTR("Static Analyzer"));
    vb->add_margin_child("scenes",scenes,true);
}
