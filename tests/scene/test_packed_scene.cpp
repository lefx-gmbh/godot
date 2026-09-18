/**************************************************************************/
/*  test_packed_scene.cpp                                                 */
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

TEST_FORCE_LINK(test_packed_scene)

#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_utils.h"

namespace TestPackedScene {

TEST_CASE("[PackedScene] Pack Scene and Retrieve State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	const Error err = packed_scene.pack(scene);
	CHECK(err == OK);

	// Retrieve the packed state.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state->get_node_count() == 1);
	CHECK(state->get_node_name(0) == "TestScene");

	memdelete(scene);
}

TEST_CASE("[PackedScene] Signals Preserved when Packing Scene") {
	// Create main scene
	// root
	// `- sub_node (local)
	// `- sub_scene (instance of another scene)
	//    `- sub_scene_node (owned by sub_scene)
	Node *main_scene_root = memnew(Node);
	Node *sub_node = memnew(Node);
	Node *sub_scene_root = memnew(Node);
	Node *sub_scene_node = memnew(Node);

	main_scene_root->add_child(sub_node);
	sub_node->set_owner(main_scene_root);

	sub_scene_root->add_child(sub_scene_node);
	sub_scene_node->set_owner(sub_scene_root);

	main_scene_root->add_child(sub_scene_root);
	sub_scene_root->set_owner(main_scene_root);

	SUBCASE("Signals that should be saved") {
		int main_flags = Object::CONNECT_PERSIST;
		// sub node to a node in main scene
		sub_node->connect("ready", callable_mp(main_scene_root, &Node::is_ready), main_flags);
		// subscene root to a node in main scene
		sub_scene_root->connect("ready", callable_mp(main_scene_root, &Node::is_ready), main_flags);
		//subscene root to subscene root (connected within main scene)
		sub_scene_root->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), main_flags);

		// Pack the scene.
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		const Error err = packed_scene->pack(main_scene_root);
		CHECK(err == OK);

		// Make sure the right connections are in packed scene.
		Ref<SceneState> state = packed_scene->get_state();
		CHECK_EQ(state->get_connection_count(), 3);
	}

	/*
	// FIXME: This subcase requires GH-48064 to be fixed.
	SUBCASE("Signals that should not be saved") {
		int subscene_flags = Object::CONNECT_PERSIST | Object::CONNECT_INHERITED;
		// subscene node to itself
		sub_scene_node->connect("ready", callable_mp(sub_scene_node, &Node::is_ready), subscene_flags);
		// subscene node to subscene root
		sub_scene_node->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), subscene_flags);
		//subscene root to subscene root (connected within sub scene)
		sub_scene_root->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), subscene_flags);

		// Pack the scene.
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		const Error err = packed_scene->pack(main_scene_root);
		CHECK(err == OK);

		// Make sure the right connections are in packed scene.
		Ref<SceneState> state = packed_scene->get_state();
		CHECK_EQ(state->get_connection_count(), 0);
	}
	*/

	memdelete(main_scene_root);
}

TEST_CASE("[PackedScene] Clear Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Clear the packed scene.
	packed_scene.clear();

	// Check if it has been cleared.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK_FALSE(state->get_node_count() == 1);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Can Instantiate Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Check if the packed scene can be instantiated.
	const bool can_instantiate = packed_scene.can_instantiate();
	CHECK(can_instantiate == true);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Instantiate Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Instantiate the packed scene.
	Node *instance = packed_scene.instantiate();
	CHECK(instance != nullptr);
	CHECK(instance->get_name() == "TestScene");

	memdelete(scene);
	memdelete(instance);
}

TEST_CASE("[PackedScene] Instantiate Packed Scene With Children") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Add persisting child nodes to the scene.
	Node *child1 = memnew(Node);
	child1->set_name("Child1");
	scene->add_child(child1);
	child1->set_owner(scene);

	Node *child2 = memnew(Node);
	child2->set_name("Child2");
	scene->add_child(child2);
	child2->set_owner(scene);

	// Add non persisting child node to the scene.
	Node *child3 = memnew(Node);
	child3->set_name("Child3");
	scene->add_child(child3);

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Instantiate the packed scene.
	Node *instance = packed_scene.instantiate();
	CHECK(instance != nullptr);
	CHECK(instance->get_name() == "TestScene");

	// Validate the child nodes of the instantiated scene.
	CHECK(instance->get_child_count() == 2);
	CHECK(instance->get_child(0)->get_name() == "Child1");
	CHECK(instance->get_child(1)->get_name() == "Child2");
	CHECK(instance->get_child(0)->get_owner() == instance);
	CHECK(instance->get_child(1)->get_owner() == instance);

	memdelete(scene);
	memdelete(instance);
}

TEST_CASE("[PackedScene] Set Path") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Set a new path for the packed scene.
	const String new_path = "NewTestPath";
	packed_scene.set_path(new_path);

	// Check if the path has been set correctly.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state->get_path() == new_path);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Replace State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Create another scene state to replace with.
	Ref<SceneState> new_state = memnew(SceneState);
	new_state->set_path("NewPath");

	// Replace the state.
	packed_scene.replace_state(new_state);

	// Check if the state has been replaced.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state == new_state);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Recreate State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(scene);

	// Recreate the state.
	packed_scene->recreate_state();

	// Check if the state has been recreated.
	Ref<SceneState> state = packed_scene->get_state();
	CHECK(state.is_valid());
	CHECK(state->get_node_count() == 0); // Since the state was recreated, it should be empty.

	memdelete(scene);
}

// -----------------------------------------------------------------------------
// A scene that is built on another scene must keep following that base until the
// user genuinely overrides something. Repacking one used to compare it against a
// base that had already been replaced, so it wrote overrides nobody asked for.
//
// Every case carries the [Editor] tag. The runner only sets Engine::is_editor_hint()
// for tagged cases (tests/test_main.cpp:242), and without it SceneState::pack() skips
// building the node path cache (packed_scene.cpp:1491). Untagged, these cases print
// node cache errors and one of them fails for that reason instead of the real one.
//
// Vocabulary used below, one word per thing:
//   base    -- the scene at the bottom, the one being edited and saved.
//   middle  -- a scene between base and outer.
//   outer   -- the scene we pack and inspect. The bug shows up in its stored state.
//   store   -- what pack() decided to write down for a node. A stored property has
//              stopped following the base.
// -----------------------------------------------------------------------------

namespace StaleBaseState {

static const Vector2 BASE_BEFORE = Vector2(100, 0);
static const Vector2 BASE_AFTER = Vector2(200, 0);

// Saves p_scene to p_path and returns the loader-cached PackedScene for it.
// Going through the loader is what makes referencing scenes share one PackedScene,
// which is the precondition for the bug.
static Ref<PackedScene> save_and_load(Node *p_scene, const String &p_path) {
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(p_scene);
	ResourceSaver::save(packed_scene, p_path);
	return ResourceLoader::load(p_path, "PackedScene");
}

static Ref<PackedScene> pack_scene(Node *p_scene) {
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(p_scene);
	return packed_scene;
}

static int find_node(const Ref<SceneState> &p_state, const String &p_node_name) {
	for (int i = 0; i < p_state->get_node_count(); i++) {
		if (String(p_state->get_node_name(i)) == p_node_name) {
			return i;
		}
	}
	return -1;
}

// Returns every property the state stored for p_node_name, sorted, comma separated.
// Comparing the whole set catches an unexpected extra property, which checking a
// single name does not.
static String stored_properties(const Ref<SceneState> &p_state, const String &p_node_name) {
	const int idx = find_node(p_state, p_node_name);
	if (idx < 0) {
		return "<node not found>";
	}
	Vector<String> names;
	for (int j = 0; j < p_state->get_node_property_count(idx); j++) {
		names.push_back(p_state->get_node_property_name(idx, j));
	}
	names.sort();
	return String(",").join(names);
}

static Variant stored_value(const Ref<SceneState> &p_state, const String &p_node_name, const String &p_property) {
	const int idx = find_node(p_state, p_node_name);
	if (idx < 0) {
		return Variant();
	}
	for (int j = 0; j < p_state->get_node_property_count(idx); j++) {
		if (String(p_state->get_node_property_name(idx, j)) == p_property) {
			return p_state->get_node_property_value(idx, j);
		}
	}
	return Variant();
}

static String stored_groups(const Ref<SceneState> &p_state, const String &p_node_name) {
	const int idx = find_node(p_state, p_node_name);
	if (idx < 0) {
		return "<node not found>";
	}
	Vector<String> names;
	for (const StringName &group : p_state->get_node_groups(idx)) {
		names.push_back(group);
	}
	names.sort();
	return String(",").join(names);
}

// Builds a new inherited scene root, the way EditorNode does when opening a scene
// with "set inherited".
static Node *make_inherited(const Ref<PackedScene> &p_base) {
	Node *root = p_base->instantiate(PackedScene::GEN_EDIT_STATE_MAIN_INHERITED);
	root->set_scene_inherited_state(p_base->get_state());
	root->set_scene_file_path(String());
	return root;
}

// Builds a sub-scene instance, the way the editor does on drop.
static Node *make_instance(const Ref<PackedScene> &p_scene) {
	Node *node = p_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	node->set_scene_file_path(p_scene->get_path());
	return node;
}

// Writes the base scene to disk and returns it. Every test uses its own path so the
// resource cache cannot carry a base from one test into the next.
static Ref<PackedScene> make_base(const String &p_path) {
	Node2D *base = memnew(Node2D);
	base->set_name("Base");
	base->set_position(BASE_BEFORE);
	Ref<PackedScene> base_ps = save_and_load(base, p_path);
	memdelete(base);
	return base_ps;
}

// Mimics the editor saving the base after a property change: open it, change the
// property, then recreate_state() + pack() into the same, shared PackedScene.
static void edit_and_save_base(const Ref<PackedScene> &p_base, const Vector2 &p_position) {
	Node2D *base_edit = Object::cast_to<Node2D>(p_base->instantiate(PackedScene::GEN_EDIT_STATE_MAIN));
	base_edit->set_position(p_position);
	p_base->recreate_state();
	p_base->pack(base_edit);
	ResourceSaver::save(p_base, p_base->get_path());
	memdelete(base_edit);
}

// Packs p_outer, writes it to p_path, and reads the whole chain back from disk.
// CACHE_MODE_IGNORE_DEEP drops the cached base and middle as well, so what comes
// back is the on-disk chain and nothing that is still alive in memory.
// The caller owns the returned node.
static Node *round_trip(Node *p_outer, const String &p_path) {
	Ref<PackedScene> outer_ps = pack_scene(p_outer);
	ResourceSaver::save(outer_ps, p_path);
	outer_ps.unref();

	Ref<PackedScene> reloaded = ResourceLoader::load(p_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
	if (reloaded.is_null()) {
		return nullptr;
	}
	return reloaded->instantiate();
}

// Position of the node at p_path below p_root, or (-1, -1) if there is no such node.
// A missing node is a failure in every case that calls this, so the sentinel only has
// to differ from every expected value.
static Vector2 position_at(Node *p_root, const NodePath &p_path) {
	Node2D *node = Object::cast_to<Node2D>(p_root->get_node_or_null(p_path));
	return node ? node->get_position() : Vector2(-1, -1);
}

static int connection_count(Node *p_node, const StringName &p_signal) {
	List<Object::Connection> connections;
	p_node->get_signal_connection_list(p_signal, &connections);
	return connections.size();
}

// base <- middle (inherits) <- outer (instances middle). The shape that fails.
struct Chain {
	Ref<PackedScene> base;
	Ref<PackedScene> middle;
	Node *outer = nullptr;
	Node2D *middle_instance = nullptr;
};

static Chain build_chain(const String &p_prefix) {
	Chain chain;
	chain.base = make_base(TestUtils::get_temp_path(p_prefix + "_base.tscn"));

	Node *middle = make_inherited(chain.base);
	middle->set_name("Middle");
	chain.middle = save_and_load(middle, TestUtils::get_temp_path(p_prefix + "_middle.tscn"));
	memdelete(middle);

	chain.outer = memnew(Node);
	chain.outer->set_name("Outer");
	chain.middle_instance = Object::cast_to<Node2D>(make_instance(chain.middle));
	chain.outer->add_child(chain.middle_instance);
	chain.middle_instance->set_owner(chain.outer);
	return chain;
}

} // namespace StaleBaseState

using namespace StaleBaseState;

// Case 1. The four nesting shapes. Only the two chained ones should be affected.
//
// Every shape asserts twice: on what pack() stored, and on the value the node actually
// has after the saved scene is read back from disk. The second assertion survives a fix
// that changes only how the same result is written down. It is also the end-to-end
// result a user sees.
TEST_CASE("[PackedScene][Editor] Stale base state: nesting matrix") {
	SUBCASE("outer inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_shapes_inh_base.tscn"));
		Node *outer = make_inherited(base_ps);
		outer->set_name("Outer");

		// Control: nothing is stored before the base is saved.
		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "Outer"), "");

		edit_and_save_base(base_ps, BASE_AFTER);

		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "Outer"), "");

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_shapes_inh_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath(".")), BASE_AFTER);

		memdelete(reloaded);
		memdelete(outer);
	}

	SUBCASE("outer instances a scene that inherits base") {
		Chain chain = build_chain("stale_shapes_ii");

		// Control: nothing is stored before the base is saved.
		CHECK_EQ(stored_properties(pack_scene(chain.outer)->get_state(), "Middle"), "");

		edit_and_save_base(chain.base, BASE_AFTER);

		CHECK_EQ(stored_properties(pack_scene(chain.outer)->get_state(), "Middle"), "");

		Node *reloaded = round_trip(chain.outer, TestUtils::get_temp_path("stale_shapes_ii_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("Middle")), BASE_AFTER);

		memdelete(reloaded);
		memdelete(chain.outer);
	}

	SUBCASE("outer instances a scene that instances base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_shapes_iins_base.tscn"));
		Node *middle = memnew(Node);
		middle->set_name("Middle");
		Node *base_instance = make_instance(base_ps);
		middle->add_child(base_instance);
		base_instance->set_owner(middle);
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_shapes_iins_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);

		// The base node belongs to the middle scene, so the outer scene stores no
		// entry for it at all.
		CHECK_EQ(find_node(pack_scene(outer)->get_state(), "Base"), -1);

		edit_and_save_base(base_ps, BASE_AFTER);

		CHECK_EQ(find_node(pack_scene(outer)->get_state(), "Base"), -1);

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_shapes_iins_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("Middle/Base")), BASE_AFTER);

		memdelete(reloaded);
		memdelete(outer);
	}

	SUBCASE("outer inherits a scene that inherits base") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_shapes_iinh_base.tscn"));
		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_shapes_iinh_middle.tscn"));
		memdelete(middle);

		Node *outer = make_inherited(middle_ps);
		outer->set_name("Outer");

		// Control: nothing is stored before the base is saved.
		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "Outer"), "");

		edit_and_save_base(base_ps, BASE_AFTER);

		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "Outer"), "");

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_shapes_iinh_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath(".")), BASE_AFTER);

		memdelete(reloaded);
		memdelete(outer);
	}
}

// Case 2. The counterweight to case 1. A fix that always answers "same" would make
// case 1 pass and silently drop real overrides. This case fails if that happens.
TEST_CASE("[PackedScene][Editor] Stale base state: overrides remain intact") {
	SUBCASE("override set on the outer scene is kept") {
		Chain chain = build_chain("stale_override_outer");
		chain.middle_instance->set_position(Vector2(300, 0));

		edit_and_save_base(chain.base, BASE_AFTER);

		Ref<SceneState> state = pack_scene(chain.outer)->get_state();
		CHECK_EQ(stored_properties(state, "Middle"), "position");
		CHECK_EQ(Vector2(stored_value(state, "Middle", "position")), Vector2(300, 0));

		Node *reloaded = round_trip(chain.outer, TestUtils::get_temp_path("stale_override_outer_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("Middle")), Vector2(300, 0));

		memdelete(reloaded);
		memdelete(chain.outer);
	}

	SUBCASE("override set in the middle scene is not copied into the outer scene") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_override_mid_base.tscn"));

		Node2D *middle = Object::cast_to<Node2D>(make_inherited(base_ps));
		middle->set_name("Middle");
		middle->set_position(Vector2(50, 0));
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_override_mid_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);

		edit_and_save_base(base_ps, BASE_AFTER);

		// The override belongs to the middle scene. The outer scene must not repeat it.
		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "Middle"), "");

		// The middle scene's own override still has to reach the instance.
		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_override_mid_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("Middle")), Vector2(50, 0));

		memdelete(reloaded);
		memdelete(outer);
	}

	SUBCASE("the middle scene keeps its own override when repacked") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_override_repack_base.tscn"));

		Node2D *middle = Object::cast_to<Node2D>(make_inherited(base_ps));
		middle->set_name("Middle");
		middle->set_position(Vector2(50, 0));

		edit_and_save_base(base_ps, BASE_AFTER);

		Ref<SceneState> state = pack_scene(middle)->get_state();
		CHECK_EQ(stored_properties(state, "Middle"), "position");
		CHECK_EQ(Vector2(stored_value(state, "Middle", "position")), Vector2(50, 0));
		memdelete(middle);
	}
}

// Case 3. get_base_scene_state() is not only used for properties. Pinning it also
// changes groups, connections and the node set, so those need their own subcases. The
// node subcases are where a wrong answer costs a node rather than a value.
TEST_CASE("[PackedScene][Editor] Stale base state: non-property scene state") {
	SUBCASE("a group changed in the base is followed by the outer scene") {
		Node2D *base = memnew(Node2D);
		base->set_name("Base");
		base->set_position(BASE_BEFORE);
		base->add_to_group("base_group", true);
		Ref<PackedScene> base_ps = save_and_load(base, TestUtils::get_temp_path("stale_group_base.tscn"));
		memdelete(base);

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_group_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);

		// Control: the group is inherited, so it is not stored before the base is saved.
		CHECK_EQ(stored_groups(pack_scene(outer)->get_state(), "Middle"), "");

		// Edit the base's groups, not only its position. Drop the group it had and
		// add another one, so that the test covers both directions.
		Node2D *base_edit = Object::cast_to<Node2D>(base_ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN));
		base_edit->set_position(BASE_AFTER);
		base_edit->remove_from_group("base_group");
		base_edit->add_to_group("late_group", true);
		base_ps->recreate_state();
		base_ps->pack(base_edit);
		ResourceSaver::save(base_ps, base_ps->get_path());
		memdelete(base_edit);

		// Still inherited, so still nothing of its own to store.
		CHECK_EQ(stored_groups(pack_scene(outer)->get_state(), "Middle"), "");

		// Both the added and the removed group have to reach the instance.
		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_group_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		Node *reloaded_middle = reloaded->get_node(NodePath("Middle"));
		CHECK(reloaded_middle->is_in_group("late_group"));
		CHECK_FALSE(reloaded_middle->is_in_group("base_group"));

		memdelete(reloaded);
		memdelete(outer);
	}

	SUBCASE("a connection removed from the base is removed from the outer scene") {
		Node2D *base = memnew(Node2D);
		base->set_name("Base");
		base->set_position(BASE_BEFORE);
		base->connect("ready", Callable(base, "is_ready"), Object::CONNECT_PERSIST);
		Ref<PackedScene> base_ps = save_and_load(base, TestUtils::get_temp_path("stale_signal_base.tscn"));
		memdelete(base);

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_signal_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);

		// Control: the connection is inherited, so it is not stored, and it is there.
		CHECK_EQ(pack_scene(outer)->get_state()->get_connection_count(), 0);
		CHECK_EQ(connection_count(middle_instance, "ready"), 1);

		// Reopen the base, drop the connection, save. The outer scene has to follow.
		Node2D *base_edit = Object::cast_to<Node2D>(base_ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN));
		base_edit->disconnect("ready", Callable(base_edit, "is_ready"));
		base_edit->set_position(BASE_AFTER);
		base_ps->recreate_state();
		base_ps->pack(base_edit);
		ResourceSaver::save(base_ps, base_ps->get_path());
		memdelete(base_edit);

		// The outer scene never touched the connection, so it must store nothing.
		CHECK_EQ(pack_scene(outer)->get_state()->get_connection_count(), 0);

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_signal_outer.tscn"));
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(connection_count(reloaded->get_node(NodePath("Middle")), "ready"), 0);

		memdelete(reloaded);
		memdelete(outer);
	}

	SUBCASE("node added to the base") {
		Chain chain = build_chain("stale_addnode");

		// Reopen the base, add a child, save.
		Node2D *base_edit = Object::cast_to<Node2D>(chain.base->instantiate(PackedScene::GEN_EDIT_STATE_MAIN));
		base_edit->set_position(BASE_AFTER);
		Node2D *extra = memnew(Node2D);
		extra->set_name("Extra");
		base_edit->add_child(extra);
		extra->set_owner(base_edit);
		chain.base->recreate_state();
		chain.base->pack(base_edit);
		ResourceSaver::save(chain.base, chain.base->get_path());
		memdelete(base_edit);

		Ref<SceneState> state = pack_scene(chain.outer)->get_state();

		// The outer scene does not have the new node in memory. It must not invent an
		// entry for it, and it must not store anything for the middle scene.
		CHECK_EQ(find_node(state, "Extra"), -1);
		CHECK_EQ(stored_properties(state, "Middle"), "");

		// The node has to arrive through the base when the outer scene is read back.
		Node *reloaded = round_trip(chain.outer, TestUtils::get_temp_path("stale_addnode_outer.tscn"));
		memdelete(chain.outer);
		REQUIRE(reloaded != nullptr);
		CHECK(reloaded->get_node_or_null(NodePath("Middle/Extra")) != nullptr);

		memdelete(reloaded);
	}

	SUBCASE("node removed from the base") {
		Node2D *base = memnew(Node2D);
		base->set_name("Base");
		base->set_position(BASE_BEFORE);
		Node2D *doomed = memnew(Node2D);
		doomed->set_name("Doomed");
		base->add_child(doomed);
		doomed->set_owner(base);
		Ref<PackedScene> base_ps = save_and_load(base, TestUtils::get_temp_path("stale_delnode_base.tscn"));
		memdelete(base);

		Node *middle = make_inherited(base_ps);
		middle->set_name("Middle");
		Ref<PackedScene> middle_ps = save_and_load(middle, TestUtils::get_temp_path("stale_delnode_middle.tscn"));
		memdelete(middle);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *middle_instance = make_instance(middle_ps);
		outer->add_child(middle_instance);
		middle_instance->set_owner(outer);

		// Control: the node is there before the base is saved.
		CHECK(outer->get_node_or_null(NodePath("Middle/Doomed")) != nullptr);

		// Reopen the base, delete the child, save.
		Node2D *base_edit = Object::cast_to<Node2D>(base_ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN));
		base_edit->set_position(BASE_AFTER);
		Node *to_remove = base_edit->get_node(NodePath("Doomed"));
		base_edit->remove_child(to_remove);
		memdelete(to_remove);
		base_ps->recreate_state();
		base_ps->pack(base_edit);
		ResourceSaver::save(base_ps, base_ps->get_path());
		memdelete(base_edit);

		// The outer scene must not resurrect the node by storing an entry for it.
		CHECK_EQ(find_node(pack_scene(outer)->get_state(), "Doomed"), -1);

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_delnode_outer.tscn"));
		memdelete(outer);
		REQUIRE(reloaded != nullptr);
		CHECK(reloaded->get_node_or_null(NodePath("Middle/Doomed")) == nullptr);

		memdelete(reloaded);
	}
}

// Case 4. Depth and repetition. The node remap walks the whole chain. Saving the base
// twice decides which old value a referencing scene then sees.
TEST_CASE("[PackedScene][Editor] Stale base state: lifecycle and depth") {
	SUBCASE("three levels of inheritance") {
		Ref<PackedScene> base_ps = make_base(TestUtils::get_temp_path("stale_deep_base.tscn"));

		Node *mid_a = make_inherited(base_ps);
		mid_a->set_name("MidA");
		Ref<PackedScene> mid_a_ps = save_and_load(mid_a, TestUtils::get_temp_path("stale_deep_mid_a.tscn"));
		memdelete(mid_a);

		Node *mid_b = make_inherited(mid_a_ps);
		mid_b->set_name("MidB");
		Ref<PackedScene> mid_b_ps = save_and_load(mid_b, TestUtils::get_temp_path("stale_deep_mid_b.tscn"));
		memdelete(mid_b);

		Node *outer = memnew(Node);
		outer->set_name("Outer");
		Node *mid_b_instance = make_instance(mid_b_ps);
		outer->add_child(mid_b_instance);
		mid_b_instance->set_owner(outer);

		// Control: nothing is stored before the base is saved.
		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "MidB"), "");

		edit_and_save_base(base_ps, BASE_AFTER);

		CHECK_EQ(stored_properties(pack_scene(outer)->get_state(), "MidB"), "");

		Node *reloaded = round_trip(outer, TestUtils::get_temp_path("stale_deep_outer.tscn"));
		memdelete(outer);
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("MidB")), BASE_AFTER);

		memdelete(reloaded);
	}

	SUBCASE("base saved twice") {
		Chain chain = build_chain("stale_twice");

		// Control: nothing is stored before the base is saved.
		CHECK_EQ(stored_properties(pack_scene(chain.outer)->get_state(), "Middle"), "");

		edit_and_save_base(chain.base, BASE_AFTER);
		edit_and_save_base(chain.base, Vector2(300, 0));

		CHECK_EQ(stored_properties(pack_scene(chain.outer)->get_state(), "Middle"), "");

		Node *reloaded = round_trip(chain.outer, TestUtils::get_temp_path("stale_twice_outer.tscn"));
		memdelete(chain.outer);
		REQUIRE(reloaded != nullptr);
		CHECK_EQ(position_at(reloaded, NodePath("Middle")), Vector2(300, 0));

		memdelete(reloaded);
	}

	// copy_from() carries the snapshot instead of resolving the base again.
	// Resolving again would make the copy follow the base as it is edited,
	// which is the bug itself.
	SUBCASE("a copied state keeps the base snapshot") {
		Chain chain = build_chain("stale_copy");

		// The base moves on first. The middle scene's snapshot stays where it was.
		// Copying only afterwards is what separates the two behaviors. Resolving the
		// base again here would give BASE_AFTER. Carrying the snapshot gives BASE_BEFORE.
		edit_and_save_base(chain.base, BASE_AFTER);
		REQUIRE_EQ(stored_value(chain.middle->get_state()->get_base_scene_state(), "Base", "position"), Variant(BASE_BEFORE));

		Ref<SceneState> copy;
		copy.instantiate();
		REQUIRE_EQ(copy->copy_from(chain.middle->get_state()), OK);

		REQUIRE(copy->get_base_scene_state().is_valid());
		CHECK_EQ(stored_value(copy->get_base_scene_state(), "Base", "position"), Variant(BASE_BEFORE));

		memdelete(chain.outer);
	}
}

} // namespace TestPackedScene
