/**************************************************************************/
/*  test_fbx.h                                                            */
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

#include "tests/test_macros.h"

#ifdef TOOLS_ENABLED

#include "../editor/editor_scene_importer_ufbx.h"

#include "scene/animation/animation_player.h"

namespace TestFBX {

TEST_CASE("[SceneTree][FBX] The animation/import option decides if an AnimationPlayer is created") {
	// The file holds one node and one animation stack without curves. Most exporters write such a stack.
	Ref<EditorSceneFormatImporterUFBX> importer;
	importer.instantiate();
	HashMap<StringName, Variant> options;
	options["fbx/importer"] = EditorSceneFormatImporterUFBX::FBX_IMPORTER_UFBX;
	options["animation/fps"] = 30;
	options["animation/trimming"] = false;

	const auto import_has_player = [&](bool p_import_animation) {
		options["animation/import"] = p_import_animation;
		Error err = OK;
		Node *scene = importer->import_scene("modules/fbx/tests/data/empty_take.fbx", p_import_animation ? EditorSceneFormatImporter::IMPORT_ANIMATION : 0, options, nullptr, &err);
		REQUIRE(err == OK);
		REQUIRE(scene != nullptr);
		const bool has_player = !scene->find_children("*", "AnimationPlayer", true, false).is_empty();
		memdelete(scene);
		return has_player;
	};

	CHECK(import_has_player(true));
	CHECK_FALSE(import_has_player(false));
}

} // namespace TestFBX

#endif // TOOLS_ENABLED
