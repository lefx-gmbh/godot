/**************************************************************************/
/*  test_editor_data.cpp                                                  */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_editor_data)

#ifdef TOOLS_ENABLED

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"
#include "editor/editor_data.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_utils.h"

// Opens EditorData::_find_updated_instances to the tests below. That function is
// private, and its only caller check_and_update_scene() goes straight on to
// reload_scene_from_memory(), which dereferences EditorNode::get_singleton() and so
// cannot run without a full editor. Detection is what these tests are about, and it
// is the half that can be tested.
class TestEditorDataInternalsAccessor {
public:
	static bool find_updated_instances(EditorData &p_editor_data, Node *p_root) {
		EditorData::SceneChainCheck check;
		return p_editor_data._find_updated_instances(p_root, p_root, check);
	}
};

namespace TestEditorData {

// The editor must reload an open scene when a scene it is built on changes on disk.
// That scene need not be the direct base. It can sit any number of steps up a chain of
// inherited scenes. A base above the first step has no node of its own in the open
// tree.
//
// Vocabulary: base is the scene that gets edited and saved, middle sits between, and
// outer is the scene open in the editor.

static const Vector2 BASE_BEFORE = Vector2(100, 0);
static const Vector2 BASE_AFTER = Vector2(200, 0);

static Ref<PackedScene> save_and_load(Node *p_scene, const String &p_path) {
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(p_scene);
	ResourceSaver::save(packed_scene, p_path);
	return ResourceLoader::load(p_path, "PackedScene");
}

static Ref<PackedScene> make_base(const String &p_path) {
	Node2D *base = memnew(Node2D);
	base->set_name("Base");
	base->set_position(BASE_BEFORE);
	Ref<PackedScene> base_ps = save_and_load(base, p_path);
	memdelete(base);
	return base_ps;
}

static Node *make_inherited(const Ref<PackedScene> &p_base) {
	Node *root = p_base->instantiate(PackedScene::GEN_EDIT_STATE_MAIN_INHERITED);
	root->set_scene_inherited_state(p_base->get_state());
	root->set_scene_file_path(String());
	return root;
}

static Node *make_instance(const Ref<PackedScene> &p_scene) {
	Node *node = p_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	node->set_scene_file_path(p_scene->get_path());
	return node;
}

// Edits a scene and writes it back, the way saving it in the editor would.
//
// Detection compares file modification time, which on some filesystems moves only once
// a second. A test writes the scene well inside that second. The save therefore repeats
// until the stamp moves. The REQUIRE reports it if the stamp never moves. Without the
// retry, nothing looks modified and every check below passes for the wrong reason.
static void edit_and_save(const Ref<PackedScene> &p_scene, const Vector2 &p_position) {
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
	REQUIRE_MESSAGE(time_moved, "Saving the scene never changed its file modification time.");
}

// Opens p_outer_ps the way EditorNode::load_scene does and registers it as the edited
// scene, returning its index.
//
// The editor has the scene open before the base is saved, so its states are snapshots
// taken at load time. Opening it afterwards instantiates against the base as already
// saved. The recorded times then agree and no shape reports a reload. That looks
// exactly like the bug, but it is not the bug.
static int open_scene(EditorData &p_editor_data, const Ref<PackedScene> &p_outer_ps) {
	Node *root = p_outer_ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
	root->set_scene_file_path(p_outer_ps->get_path());

	const int idx = p_editor_data.add_edited_scene(-1);
	p_editor_data.set_edited_scene(idx);
	p_editor_data.set_edited_scene_root(root);
	return idx;
}

TEST_CASE("[EditorData][Editor] Changing a base scene marks the open scene for reload") {
	SUBCASE("outer inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_a_base.tscn"));

		Node *outer = make_inherited(base_ps);
		outer->set_name("Outer");
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_a_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);
		edit_and_save(base_ps, BASE_AFTER);

		INFO("The base is the direct base of the open scene.");
		CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

		editor_data.clear_edited_scenes();
	}

	SUBCASE("outer instances a scene that inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_b_base.tscn"));

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("ed_b_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_b_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);
		edit_and_save(base_ps, BASE_AFTER);

		INFO("The base is one step above the instanced scene, so no node holds its state.");
		CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

		editor_data.clear_edited_scenes();
	}

	SUBCASE("outer instances a scene that instances base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_c_base.tscn"));

		Node *middle = memnew(Node);
		middle->set_name("Middle");
		Node *base_instance = make_instance(base_ps);
		base_instance->set_name("Base");
		middle->add_child(base_instance);
		base_instance->set_owner(middle);
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("ed_c_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_c_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);
		edit_and_save(base_ps, BASE_AFTER);

		INFO("The base is instanced rather than inherited, so it keeps a node of its own.");
		CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

		editor_data.clear_edited_scenes();
	}

	SUBCASE("outer inherits a scene that inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_d_base.tscn"));

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("ed_d_middle.tscn"));
		memdelete(middle);

		Node *outer = make_inherited(middle_ps);
		outer->set_name("Outer");
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_d_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);
		edit_and_save(base_ps, BASE_AFTER);

		INFO("The base is two inheritance steps above the open scene.");
		CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

		editor_data.clear_edited_scenes();
	}
}

// Builds p_depth scenes, each one inheriting the one before it, and returns them from
// the deepest to the shallowest.
static Vector<Ref<PackedScene>> make_inheritance_chain(const String &p_tag, int p_depth) {
	Vector<Ref<PackedScene>> levels;
	levels.push_back(make_base(TestUtils::get_temp_path(p_tag + "_0.tscn")));

	for (int level = 1; level < p_depth; level++) {
		Node *node = make_inherited(levels[level - 1]);
		node->set_name("Level" + itos(level));
		levels.push_back(save_and_load(node, TestUtils::get_temp_path(p_tag + "_" + itos(level) + ".tscn")));
		memdelete(node);
	}

	return levels;
}

// Opens a scene inheriting the top of a four scene chain, edits the level named by
// p_edited_level, and reports whether a reload is asked for.
static bool reload_wanted_after_editing(const String &p_tag, int p_edited_level) {
	Vector<Ref<PackedScene>> levels = make_inheritance_chain(p_tag, 4);

	Node *outer = make_inherited(levels[levels.size() - 1]);
	outer->set_name("Outer");
	Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path(p_tag + "_outer.tscn"));
	memdelete(outer);

	EditorData editor_data;
	const int idx = open_scene(editor_data, outer_ps);
	edit_and_save(levels[p_edited_level], Vector2(200 + p_edited_level, 0));

	const bool wants_reload = TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx));
	editor_data.clear_edited_scenes();
	return wants_reload;
}

// Comparing the two ends of a chain catches a chain only two scenes deep. A longer
// chain is needed to show that the walk really checks every step.
TEST_CASE("[EditorData][Editor] A change is found at every level of a longer chain") {
	SUBCASE("the deepest scene changed") {
		CHECK(reload_wanted_after_editing("ed_f0", 0));
	}

	SUBCASE("the second scene changed") {
		CHECK(reload_wanted_after_editing("ed_f1", 1));
	}

	SUBCASE("the third scene changed") {
		CHECK(reload_wanted_after_editing("ed_f2", 2));
	}

	SUBCASE("the direct base changed") {
		CHECK(reload_wanted_after_editing("ed_f3", 3));
	}
}

// The same pass handles inheriting and instancing in different parts. An instanced
// scene puts a node in the tree, and the walk over children finds it. An inherited
// scene puts no node there. The walk reaches it by climbing base states. Alternating
// them checks that a chain can change from one to the other at every step.
TEST_CASE("[EditorData][Editor] A change is found through mixed inheriting and instancing") {
	Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_g_base.tscn"));

	Node *first = make_inherited(base_ps);
	first->set_name("First");
	Ref<PackedScene> first_ps = save_and_load(first, TestUtils::get_temp_path("ed_g_first.tscn"));
	memdelete(first);

	Node *second = memnew(Node);
	second->set_name("Second");
	Node *first_instance = make_instance(first_ps);
	second->add_child(first_instance);
	first_instance->set_owner(second);
	Ref<PackedScene> second_ps = save_and_load(second, TestUtils::get_temp_path("ed_g_second.tscn"));
	memdelete(second);

	Node *third = make_inherited(second_ps);
	third->set_name("Third");
	Ref<PackedScene> third_ps = save_and_load(third, TestUtils::get_temp_path("ed_g_third.tscn"));
	memdelete(third);

	Node *outer = memnew(Node);
	outer->set_name("Outer");
	Node *third_instance = make_instance(third_ps);
	outer->add_child(third_instance);
	third_instance->set_owner(outer);
	Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_g_outer.tscn"));
	memdelete(outer);

	EditorData editor_data;
	const int idx = open_scene(editor_data, outer_ps);
	edit_and_save(base_ps, BASE_AFTER);

	INFO("The base is reached by inheriting, instancing, inheriting and instancing again.");
	CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

	editor_data.clear_edited_scenes();
}

// A wide tree reaches the same base through more than one branch. Where both branches
// hold the same state, the second pass over it is redundant. The case below covers the
// harder shape, where the branches hold different states of the one file.
TEST_CASE("[EditorData][Editor] A shared base is found through either branch") {
	Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_h_base.tscn"));

	Node *left = make_inherited(base_ps);
	left->set_name("Left");
	Ref<PackedScene> left_ps = save_and_load(left, TestUtils::get_temp_path("ed_h_left.tscn"));
	memdelete(left);

	Node *right = make_inherited(base_ps);
	right->set_name("Right");
	Ref<PackedScene> right_ps = save_and_load(right, TestUtils::get_temp_path("ed_h_right.tscn"));
	memdelete(right);

	Node *outer = memnew(Node);
	outer->set_name("Outer");
	Node *left_instance = make_instance(left_ps);
	left_instance->set_name("LeftInstance");
	outer->add_child(left_instance);
	left_instance->set_owner(outer);
	Node *right_instance = make_instance(right_ps);
	right_instance->set_name("RightInstance");
	outer->add_child(right_instance);
	right_instance->set_owner(outer);
	Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_h_outer.tscn"));
	memdelete(outer);

	EditorData editor_data;
	const int idx = open_scene(editor_data, outer_ps);
	edit_and_save(base_ps, BASE_AFTER);

	INFO("One base sits under two sibling branches of the open scene.");
	CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

	editor_data.clear_edited_scenes();
}

// Two branches of one tree can hold different snapshots of the same base scene, one
// from before a save and one from after. The walk therefore cannot note a base as done
// by its path. The branch that noted it says nothing about the state the other branch
// holds.
TEST_CASE("[EditorData][Editor] A stale branch is found although a current branch shares its base") {
	const String base_path = TestUtils::get_temp_path("ed_p_base.tscn");
	Ref<PackedScene> base_ps = make_base(base_path);

	// Left captures the base as it stands now and keeps that snapshot alive.
	Node *left = make_inherited(base_ps);
	left->set_name("Left");
	Ref<PackedScene> left_ps = save_and_load(left, TestUtils::get_temp_path("ed_p_left.tscn"));
	memdelete(left);

	edit_and_save(base_ps, BASE_AFTER);

	// What the editor's filesystem scan does when it notices the file changed.
	Ref<PackedScene> reloaded = ResourceLoader::load(base_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_REPLACE);

	Node *right = make_inherited(base_ps);
	right->set_name("Right");
	Ref<PackedScene> right_ps = save_and_load(right, TestUtils::get_temp_path("ed_p_right.tscn"));
	memdelete(right);

	// The scenario only means something if the two branches really do hold separate
	// states of the one base. The later state must be current and the earlier one stale.
	const uint64_t file_time = FileAccess::get_modified_time(base_path);
	Ref<SceneState> left_base = left_ps->get_state()->get_base_scene_state();
	Ref<SceneState> right_base = right_ps->get_state()->get_base_scene_state();
	REQUIRE(left_base.is_valid());
	REQUIRE(right_base.is_valid());
	REQUIRE_MESSAGE(left_base != right_base, "Both branches ended up on one state, so nothing is being tested.");
	REQUIRE_MESSAGE(right_base->get_last_modified_time() == file_time, "The branch checked first is not current, so it would report the change itself.");
	REQUIRE_MESSAGE(left_base->get_last_modified_time() != file_time, "The other branch is not stale, so there is nothing to miss.");

	Node *outer = memnew(Node);
	outer->set_name("Outer");
	// The current branch goes first, so it is the one that passes and notes the path.
	Node *right_instance = make_instance(right_ps);
	right_instance->set_name("RightInstance");
	outer->add_child(right_instance);
	right_instance->set_owner(outer);
	Node *left_instance = make_instance(left_ps);
	left_instance->set_name("LeftInstance");
	outer->add_child(left_instance);
	left_instance->set_owner(outer);
	Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_p_outer.tscn"));
	memdelete(outer);

	EditorData editor_data;
	const int idx = open_scene(editor_data, outer_ps);

	INFO("The second branch still holds the base as it was before the save.");
	CHECK(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

	editor_data.clear_edited_scenes();
}

TEST_CASE("[EditorData][Editor] An unchanged base scene leaves the open scene alone") {
	Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_e_base.tscn"));

	Node *middle = make_inherited(base_ps);
	middle->set_name("Middle");
	Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("ed_e_middle.tscn"));
	memdelete(middle);

	Node *outer = make_inherited(middle_ps);
	outer->set_name("Outer");
	Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_e_outer.tscn"));
	memdelete(outer);

	EditorData editor_data;
	const int idx = open_scene(editor_data, outer_ps);

	INFO("Nothing was saved, so walking the chain must not ask for a reload.");
	CHECK_FALSE(TestEditorDataInternalsAccessor::find_updated_instances(editor_data, editor_data.get_edited_scene_root(idx)));

	editor_data.clear_edited_scenes();
}

} // namespace TestEditorData

#endif // TOOLS_ENABLED
