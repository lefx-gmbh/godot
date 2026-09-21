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
#include "editor/editor_data.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/editor/test_scene_inheritance_fixtures.h"
#include "tests/test_utils.h"

namespace TestEditorData {

using namespace TestSceneInheritance;

// The editor must reload an open scene when a scene it is built on changes on disk.
// That scene need not be the direct base. It can sit any number of steps up a chain of
// inherited scenes. A base above the first step has no node of its own in the open
// tree.

TEST_CASE("[EditorData][Editor] Changing a base scene marks the open scene for reload") {
	SUBCASE("outer inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("ed_a_base.tscn"));

		Node *outer = make_inherited(base_ps);
		outer->set_name("Outer");
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("ed_a_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);
		edit_and_save_checked(base_ps, BASE_AFTER);

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
		edit_and_save_checked(base_ps, BASE_AFTER);

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
		edit_and_save_checked(base_ps, BASE_AFTER);

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
		edit_and_save_checked(base_ps, BASE_AFTER);

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
	edit_and_save_checked(levels[p_edited_level], Vector2(200 + p_edited_level, 0));

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
	edit_and_save_checked(base_ps, BASE_AFTER);

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
	edit_and_save_checked(base_ps, BASE_AFTER);

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

	Node *left = make_inherited(base_ps);
	left->set_name("Left");
	Ref<PackedScene> left_ps = save_and_load(left, TestUtils::get_temp_path("ed_p_left.tscn"));
	memdelete(left);

	Node *right = make_inherited(base_ps);
	right->set_name("Right");
	Ref<PackedScene> right_ps = save_and_load(right, TestUtils::get_temp_path("ed_p_right.tscn"));
	memdelete(right);

	// The left branch is built while the base still says what it said at load time, which
	// is what makes it stale rather than merely bookkept as stale. Building both branches
	// after the save, as instantiating the whole outer scene from disk would, gives two
	// branches that are equally current and leaves nothing for the walk to find.
	Node *left_instance = make_instance(left_ps);
	left_instance->set_name("LeftInstance");

	edit_and_save_checked(base_ps, BASE_AFTER);

	// What the editor's filesystem scan does when it notices the file changed.
	Ref<PackedScene> reloaded = ResourceLoader::load(base_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_REPLACE);

	// The right branch is built afterwards, so it picks the base up as it now stands.
	Node *right_instance = make_instance(right_ps);
	right_instance->set_name("RightInstance");

	// The scenario only means something if the two branches really do hold separate
	// states of the one base. The later one must be current and the earlier one stale.
	const uint64_t file_time = FileAccess::get_modified_time(base_path);
	Ref<SceneState> left_base = left_instance->get_scene_instance_state()->get_base_scene_state();
	Ref<SceneState> right_base = right_instance->get_scene_instance_state()->get_base_scene_state();
	REQUIRE(left_base.is_valid());
	REQUIRE(right_base.is_valid());
	REQUIRE_MESSAGE(left_base != right_base, "Both branches ended up on one state, so nothing is being tested.");
	REQUIRE_MESSAGE(right_base->get_last_modified_time() == file_time, "The branch checked first is not current, so it would report the change itself.");
	REQUIRE_MESSAGE(left_base->get_last_modified_time() != file_time, "The other branch is not stale, so there is nothing to miss.");

	// The tree is assembled from the branches already built rather than instantiated from
	// a file, because the order the two branches were built in is the point of the test.
	// The current branch is added first, so it is the one that passes and notes the path.
	Node *outer = memnew(Node);
	outer->set_name("Outer");
	outer->add_child(right_instance);
	right_instance->set_owner(outer);
	outer->add_child(left_instance);
	left_instance->set_owner(outer);

	EditorData editor_data;
	const int idx = editor_data.add_edited_scene(-1);
	editor_data.set_edited_scene(idx);
	editor_data.set_edited_scene_root(outer);

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
