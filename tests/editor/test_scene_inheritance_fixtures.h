/**************************************************************************/
/*  test_scene_inheritance_fixtures.h                                     */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "editor/editor_data.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"

// Opens EditorData::_find_updated_instances to the tests. That function is private, and
// its only caller check_and_update_scene() goes straight on to reload_scene_from_memory(),
// which dereferences EditorNode::get_singleton() and so cannot run without a full editor.
// Reaching the detection pass directly keeps the half that can be tested headless, and
// keeps the tests asserting against the engine rather than against a copy of it.
class TestEditorDataInternalsAccessor {
public:
	static bool find_updated_instances(EditorData &p_editor_data, Node *p_root) {
		EditorData::SceneChainCheck check;
		return p_editor_data._find_updated_instances(p_root, p_root, check);
	}
};

// Builders for the scene shapes the reload tests are made of: a base scene, scenes that
// inherit or instance it, and the saving and opening order those tests depend on.
//
// Vocabulary: base is the scene that gets edited and saved, middle sits between, and
// outer is the scene open in the editor.
namespace TestSceneInheritance {

// The position a freshly built base scene carries before anything is saved over it.
static const Vector2 BASE_BEFORE = Vector2(100, 0);
static const Vector2 BASE_AFTER = Vector2(200, 0);

inline Ref<PackedScene> save_and_load(Node *p_scene, const String &p_path) {
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(p_scene);
	ResourceSaver::save(packed_scene, p_path);
	return ResourceLoader::load(p_path, "PackedScene");
}

// p_position is a parameter because a test driving the base through several values wants
// to name its own starting point rather than have it implied by a shared constant.
inline Ref<PackedScene> make_base(const String &p_path, const Vector2 &p_position = BASE_BEFORE) {
	Node2D *base = memnew(Node2D);
	base->set_name("Base");
	base->set_position(p_position);
	Ref<PackedScene> base_ps = save_and_load(base, p_path);
	memdelete(base);
	return base_ps;
}

inline Node *make_inherited(const Ref<PackedScene> &p_base) {
	Node *root = p_base->instantiate(PackedScene::GEN_EDIT_STATE_MAIN_INHERITED);
	root->set_scene_inherited_state(p_base->get_state());
	root->set_scene_file_path(String());
	return root;
}

inline Node *make_instance(const Ref<PackedScene> &p_scene) {
	Node *node = p_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	node->set_scene_file_path(p_scene->get_path());
	return node;
}

// Edits a scene and writes it back, the way saving it in the editor would. Returns
// whether the file modification time actually moved.
//
// Detection compares file modification time, which on some filesystems moves only once a
// second. A test writes the scene well inside that second. The save therefore repeats
// until the stamp moves. Without the retry, nothing looks modified and every check passes
// for the wrong reason, which is indistinguishable from the bug. The return value is left
// to the caller so that a test can decide how loudly to complain.
inline bool edit_and_save(const Ref<PackedScene> &p_scene, const Vector2 &p_position) {
	const String path = p_scene->get_path();
	const uint64_t before = FileAccess::get_modified_time(path);

	Node *edited = p_scene->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
	Node2D *edited_2d = Object::cast_to<Node2D>(edited);
	if (edited_2d) {
		edited_2d->set_position(p_position);
	}
	p_scene->recreate_state();
	p_scene->pack(edited);

	bool time_moved = false;
	for (int attempt = 0; attempt < 30; attempt++) {
		ResourceSaver::save(p_scene, path);
		if (FileAccess::get_modified_time(path) != before) {
			time_moved = true;
			break;
		}
		OS::get_singleton()->delay_usec(100000);
	}

	memdelete(edited);
	return time_moved;
}

// Saves the scene and insists that the file stamp really moved. A save that leaves the
// stamp alone makes every check that follows pass for the wrong reason, so it is
// reported here rather than left to be read as a passing test.
inline void edit_and_save_checked(const Ref<PackedScene> &p_scene, const Vector2 &p_position) {
	REQUIRE_MESSAGE(edit_and_save(p_scene, p_position), "Saving the scene never changed its file modification time.");
}

// Opens p_outer_ps the way EditorNode::load_scene does and registers it as the edited
// scene, returning its index.
//
// Order matters and is the whole point: the editor has the scene open before the base is
// saved, so its states are snapshots taken at load time. Opening it afterwards
// instantiates against the base as already saved. The recorded times then agree and no
// shape reports a reload. That looks exactly like the bug, but it is not the bug.
inline int open_scene(EditorData &p_editor_data, const Ref<PackedScene> &p_outer_ps) {
	Node *root = p_outer_ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
	root->set_scene_file_path(p_outer_ps->get_path());

	const int idx = p_editor_data.add_edited_scene(-1);
	p_editor_data.set_edited_scene(idx);
	p_editor_data.set_edited_scene_root(root);
	return idx;
}

} // namespace TestSceneInheritance

#endif // TOOLS_ENABLED
