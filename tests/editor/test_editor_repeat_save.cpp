/**************************************************************************/
/*  test_editor_repeat_save.cpp                                           */
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

TEST_FORCE_LINK(test_editor_repeat_save)

#ifdef TOOLS_ENABLED

#include "editor/editor_data.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/editor/test_scene_inheritance_fixtures.h"
#include "tests/test_utils.h"

// The base scene is edited and saved repeatedly while a derived scene sits open in an
// editor tab. test_editor_data.cpp stops at the question the detection pass answers,
// which is whether the editor notices the save. This one carries on through the reload
// itself, because that is where the damage happens.
//
// Two things have to be checked, and only one of them is visible to a user at a glance:
//
//   1. The reloaded node still shows the current base value.
//   2. The reloaded scene does not STORE that value as an override of its own.
//
// The second is the real defect. Once the scene stores its own `position`, it has
// stopped following the base, and the next base save leaves it stale. A single round
// cannot tell the two apart -- after one save the stored override and the base value
// are both 200 and look identical -- so every shape here is driven through several
// rounds, 100 -> 200 -> 300 -> 400. The stored-override check is what makes even
// round 1 meaningful.
//
// The reload performed below is EditorData::reload_scene_from_memory (editor_data.cpp)
// minus its one editor-only line. That function packs the live root, instantiates the
// pack as a main scene, and hands the result to EditorNode::set_edited_scene. Only that
// last call needs a running editor; the pack-and-instantiate pair IS the reload, so it
// is done here directly and the result registered through EditorData::set_edited_scene_root.
// Its intermediate PackedScene is also exactly what a subsequent "Save Scene" would
// write to disk, which is why the override check reads that state rather than the file.

namespace TestEditorRepeatSave {

using namespace TestSceneInheritance;

// Four rounds: the value the base starts at, then one save per further entry. Shapes a
// and c are the in-test control and must stay clean through all of them.
static const Vector2 ROUND_POSITIONS[] = {
	Vector2(100, 0),
	Vector2(200, 0),
	Vector2(300, 0),
	Vector2(400, 0),
};
static const int ROUND_COUNT = 4;

// EditorData::reload_scene_from_memory without its EditorNode line. r_packed keeps the
// pack alive because it is the artifact the assertions read: what "Save Scene" would put
// on disk right now.
static Node *reload_edited_scene(EditorData &p_editor_data, int p_idx, Ref<PackedScene> &r_packed) {
	Node *old_root = p_editor_data.get_edited_scene_root(p_idx);

	r_packed.instantiate();
	// Pack first, so it stores diffs to the previous version of the saved scene.
	REQUIRE(r_packed->pack(old_root) == OK);
	Node *new_scene = r_packed->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
	REQUIRE(new_scene != nullptr);

	new_scene->set_scene_file_path(old_root->get_scene_file_path());
	p_editor_data.set_edited_scene_root(new_scene);
	memdelete(old_root);
	return new_scene;
}

// Outer never overrides `position` anywhere -- no round of this test ever touches a node
// in the outer scene. So any stored `position` in its state is the scene having quietly
// detached from the base, which is the Inspector's revert arrow seen by hand.
static void check_no_stored_position(const Ref<PackedScene> &p_packed, const String &p_shape, int p_round) {
	Ref<SceneState> state = p_packed->get_state();
	REQUIRE(state.is_valid());

	for (int i = 0; i < state->get_node_count(); i++) {
		for (int p = 0; p < state->get_node_property_count(i); p++) {
			if (state->get_node_property_name(i, p) != StringName("position")) {
				continue;
			}
			CHECK_MESSAGE(false, vformat("shape %s, round %d: node '%s' stores its own position = %s", p_shape, p_round, state->get_node_path(i), state->get_node_property_value(i, p)));
		}
	}
}

// One shape, driven through every round. p_follower is the node inside outer that is
// supposed to keep following the base.
static void run_rounds(EditorData &p_editor_data, int p_idx, const Ref<PackedScene> &p_base_ps,
		const NodePath &p_follower, const String &p_shape) {
	for (int round = 1; round < ROUND_COUNT; round++) {
		edit_and_save_checked(p_base_ps, ROUND_POSITIONS[round]);

		Node *root = p_editor_data.get_edited_scene_root(p_idx);
		REQUIRE(root != nullptr);

		INFO("shape ", p_shape, ", round ", round);
		CHECK_MESSAGE(TestEditorDataInternalsAccessor::find_updated_instances(p_editor_data, root),
				vformat("shape %s, round %d: the editor would not even notice the base save", p_shape, round));

		Ref<PackedScene> packed;
		root = reload_edited_scene(p_editor_data, p_idx, packed);

		// The override check comes first: it is the defect, and it is true even in the
		// round where the displayed value happens to agree with the base.
		check_no_stored_position(packed, p_shape, round);

		Node2D *follower = Object::cast_to<Node2D>(root->get_node_or_null(p_follower));
		REQUIRE(follower != nullptr);
		CHECK_MESSAGE(follower->get_position() == ROUND_POSITIONS[round],
				vformat("shape %s, round %d: follower shows %s, base is %s",
						p_shape, round, String(follower->get_position()),
						String(ROUND_POSITIONS[round])));
	}
}

TEST_CASE("[EditorData][Editor] Repeated base scene saves keep derived scenes following the base") {
	SUBCASE("a: outer inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("rs_a_base.tscn"), ROUND_POSITIONS[0]);

		Node *outer = make_inherited(base_ps);
		outer->set_name("Outer");
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("rs_a_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);

		INFO("direct inheritance, reported working by hand -- in-test control");
		run_rounds(editor_data, idx, base_ps, NodePath("."), "a");

		editor_data.clear_edited_scenes();
	}

	SUBCASE("b: outer instances a scene that inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("rs_b_base.tscn"), ROUND_POSITIONS[0]);

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("rs_b_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("rs_b_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);

		INFO("chained base through instancing, broken by hand from round 2");
		run_rounds(editor_data, idx, base_ps, NodePath("Middle"), "b");

		editor_data.clear_edited_scenes();
	}

	SUBCASE("c: outer instances a scene that instances base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("rs_c_base.tscn"), ROUND_POSITIONS[0]);

		Node *middle = memnew(Node);
		middle->set_name("Middle");
		Node *base_instance = make_instance(base_ps);
		base_instance->set_name("Base");
		middle->add_child(base_instance);
		base_instance->set_owner(middle);
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("rs_c_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("rs_c_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);

		INFO("plain instancing, never reported broken -- in-test control");
		run_rounds(editor_data, idx, base_ps, NodePath("Middle/Base"), "c");

		editor_data.clear_edited_scenes();
	}

	SUBCASE("d: outer inherits a scene that inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("rs_d_base.tscn"), ROUND_POSITIONS[0]);

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("rs_d_middle.tscn"));
		memdelete(middle);

		Node *outer = make_inherited(middle_ps);
		outer->set_name("Outer");
		Ref<PackedScene> outer_ps = save_and_load(outer, TestUtils::get_temp_path("rs_d_outer.tscn"));
		memdelete(outer);

		EditorData editor_data;
		const int idx = open_scene(editor_data, outer_ps);

		INFO("chained base through inheritance, broken by hand from round 2");
		run_rounds(editor_data, idx, base_ps, NodePath("."), "d");

		editor_data.clear_edited_scenes();
	}
}

} // namespace TestEditorRepeatSave

#endif // TOOLS_ENABLED
