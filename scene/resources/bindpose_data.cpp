/**************************************************************************/
/*  bindpose_data.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "bindpose_data.h"
#include "scene/3d/skeleton_3d.h"
#include <core/io/config_file.h>

bool BindposeData::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;
	if (path == "source_pose") {
		source_pose = p_value;
		return true;
	}
	if (path == "store_source_bindpose") {
		store_source_bindpose = p_value;
		return true;
	}
	if (path == "load_source_bindpose_from_other") {
		load_source_bindpose_from_other = p_value;
		return true;
	}
	return false;
}

bool BindposeData::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;
	if (path == "source_pose") {
		r_ret = source_pose;
		return true;
	}
	if (path == "store_source_bindpose") {
		r_ret = store_source_bindpose;
		return true;
	}
	if (path == "load_source_bindpose_from_other") {
		r_ret = load_source_bindpose_from_other;
		return true;
	}
	return false;
}

void BindposeData::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::BOOL, "store_source_bindpose", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED));
	if (!store_source_bindpose) {
		p_list->push_back(PropertyInfo(Variant::STRING, "load_source_bindpose_from_other", PROPERTY_HINT_FILE, "*.import, *.res", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED));
	}
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "source_pose", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
}


Dictionary BindposeData::get_referenced_pose(Node *p_root, Node *p_skeleton) {
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_skeleton);
	Dictionary empty;
	print_line("get ref pose");
	if (!skeleton) {
		print_error("Value passed in that was not a Skeleton3D.");
		return empty;
	}
	String target_import_path = load_source_bindpose_from_other;
	if (!load_source_bindpose_from_other.is_empty()) {
		if (!FileAccess::exists(target_import_path)) {
			print_error("Could find file at: " + target_import_path);
		} else {
			Ref<ConfigFile> cf;
			cf.instantiate();
			Error err = cf->load(target_import_path);
			if (err == OK && cf->has_section_key("params", "_subresources")) {
				Dictionary subresources = cf->get_value("params", "_subresources");
				print_line("get params");
				Dictionary nodes;
				if (subresources.has("nodes")) {
					nodes = subresources["nodes"];
					String skeleton_path = "PATH:" + p_root->get_path_to(p_skeleton);
					print_line("found skel");
					print_line(skeleton_path);

					if (nodes.has(skeleton_path)) {
						print_line("has skelly path");
						Dictionary skeleton_data = nodes[skeleton_path];
						if (skeleton_data.has("import/bindpose_data")) {
							print_line("skelly has bindpose");
							BindposeData *bindpose_data = Object::cast_to<BindposeData>(skeleton_data["import/bindpose_data"]);
							print_line(bindpose_data->store_source_bindpose);
							return bindpose_data->source_pose;
						}
						
					}
				}
			}
		}
	}
	return empty;
}

void BindposeData::process_skeleton(Node *p_root, Node *p_skeleton) {
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_skeleton);
	if (!skeleton) {
		print_error("Value passed in that was not a Skeleton3D.");
		return;
	}
	if (store_source_bindpose) {
		source_pose["basis"] = skeleton->get_basis();

		int bone_count = skeleton->get_bone_count();
		for (int i = 0; i < bone_count; i++) {
			source_pose[skeleton->get_bone_name(i)] = skeleton->get_bone_rest(i);
		}
	}
	Dictionary &referenced_pose = get_referenced_pose(p_root, p_skeleton);
	if (!referenced_pose.is_empty()) {
		Basis skeleton_basis = referenced_pose["basis"];
		skeleton->set_basis(skeleton_basis);

		int bone_count = skeleton->get_bone_count();
		for (int b = 0; b < bone_count; b++) {
			String bone_name = skeleton->get_bone_name(b);
			if (referenced_pose.has(bone_name)) {
				print_line("Changed bone: " + bone_name);
				Transform3D pose = referenced_pose[bone_name];
				skeleton->set_bone_rest(b, pose);
				skeleton->set_bone_pose_rotation(b, pose.basis.get_quaternion());
				skeleton->set_bone_global_pose_override(b, pose, true);
			}
		}
		//skeleton->reset_bone_poses();
	}
	notify_property_list_changed();
}

void BindposeData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("process_skeleton", "p_root", "skeleton"), &BindposeData::process_skeleton);
	ClassDB::bind_method(D_METHOD("get_referenced_pose", "p_root", "skeleton"), &BindposeData::get_referenced_pose);
	ADD_SIGNAL(MethodInfo("source_bindpose_updated"));
}

void BindposeData::_validate_property(PropertyInfo &property) const {
	if (property.name == "import/bindpose_data") {
		property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

BindposeData::BindposeData() {
	store_source_bindpose = false;
}

BindposeData::~BindposeData() {
}

//////////////////////////////////////
