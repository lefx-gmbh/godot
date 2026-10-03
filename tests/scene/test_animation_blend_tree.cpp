/**************************************************************************/
/*  test_animation_blend_tree.cpp                                         */
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

TEST_FORCE_LINK(test_animation_blend_tree)

#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"

namespace TestAnimationBlendTree {

TEST_CASE("[SceneTree][AnimationBlendTree] Create AnimationBlendTree and add AnimationNode") {
	Ref<AnimationNodeBlendTree> blend_tree;
	blend_tree.instantiate();

	// Test initial state.
	CHECK(blend_tree->has_node("output"));
	CHECK_EQ(blend_tree->get_graph_offset(), Vector2(0, 0));
	CHECK_EQ(blend_tree->get_node_list().size(), 1);

	// Test adding animation node.
	Ref<AnimationNodeAnimation> anim_node;
	anim_node.instantiate();
	anim_node->set_animation(StringName("test_animation"));
	Vector2 position(100, 100);
	blend_tree->add_node("test_node", anim_node, position);

	// Test node existence.
	CHECK(blend_tree->has_node("test_node"));
	CHECK_EQ(blend_tree->get_node("test_node"), anim_node);
	CHECK_EQ(blend_tree->get_node_position("test_node"), position);

	// Test node connection on port 0.
	CHECK_EQ(blend_tree->can_connect_node("output", 0, "test_node"), AnimationNodeBlendTree::CONNECTION_OK);
	blend_tree->connect_node("output", 0, "test_node");

	const LocalVector<StringName> *connections = blend_tree->get_node_connection_array("output");
	CHECK_EQ(connections->size(), 1);
	CHECK_EQ(connections->operator[](0), StringName("test_node"));

	// Test node rename.
	blend_tree->rename_node("test_node", "renamed_node");
	CHECK_FALSE(blend_tree->has_node("test_node"));
	CHECK(blend_tree->has_node("renamed_node"));

	connections = blend_tree->get_node_connection_array("output");
	CHECK_EQ(connections->operator[](0), StringName("renamed_node"));

	// Test node removal.
	blend_tree->remove_node("renamed_node");
	CHECK_FALSE(blend_tree->has_node("renamed_node"));

	connections = blend_tree->get_node_connection_array("output");
	CHECK_EQ(connections->operator[](0), StringName());
}

TEST_CASE("[SceneTree][AnimationBlendTree] Replace existing child node through storage property") {
	SUBCASE("AnimationNodeBlendTree keeps position and connections") {
		Ref<AnimationNodeBlendTree> blend_tree;
		blend_tree.instantiate();
		Ref<AnimationNodeAnimation> anim_node;
		anim_node.instantiate();
		blend_tree->add_node("A", anim_node, Vector2(10, 20));
		blend_tree->connect_node("output", 0, "A");

		Ref<AnimationNodeAnimation> copy = anim_node->duplicate();
		bool valid = false;
		blend_tree->set("nodes/A/node", copy, &valid);
		CHECK(valid);
		CHECK_EQ(blend_tree->get_node("A"), copy);
		CHECK_EQ(blend_tree->get_node_position("A"), Vector2(10, 20));
		const LocalVector<StringName> *connections = blend_tree->get_node_connection_array("output");
		REQUIRE_EQ(connections->size(), 1);
		CHECK_EQ(connections->operator[](0), StringName("A"));

		// The output node must not be replaceable.
		Ref<AnimationNode> output = blend_tree->get_node("output");
		Ref<AnimationNodeOutput> other_output;
		other_output.instantiate();
		ERR_PRINT_OFF;
		blend_tree->set("nodes/output/node", other_output);
		ERR_PRINT_ON;
		CHECK_EQ(blend_tree->get_node("output"), output);
	}

	SUBCASE("AnimationNodeStateMachine keeps position and transitions") {
		Ref<AnimationNodeStateMachine> state_machine;
		state_machine.instantiate();
		Ref<AnimationNodeAnimation> state_s;
		state_s.instantiate();
		Ref<AnimationNodeAnimation> state_t;
		state_t.instantiate();
		state_machine->add_node("S", state_s, Vector2(30, 40));
		state_machine->add_node("T", state_t, Vector2(50, 60));
		Ref<AnimationNodeStateMachineTransition> transition;
		transition.instantiate();
		state_machine->add_transition("S", "T", transition);

		Ref<AnimationNodeAnimation> copy = state_s->duplicate();
		bool valid = false;
		state_machine->set("states/S/node", copy, &valid);
		CHECK(valid);
		CHECK_EQ(state_machine->get_node("S"), copy);
		CHECK_EQ(state_machine->get_node_position("S"), Vector2(30, 40));
		CHECK(state_machine->has_transition("S", "T"));
	}
}

} // namespace TestAnimationBlendTree
