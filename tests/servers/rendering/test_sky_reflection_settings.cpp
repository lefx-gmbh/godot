/**************************************************************************/
/*  test_sky_reflection_settings.cpp                                      */
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

TEST_FORCE_LINK(test_sky_reflection_settings)

#include "core/config/project_settings.h"

namespace TestSkyReflectionSettings {

TEST_CASE("[ProjectSettings] Accurate rough reflections are on by default and need a restart") {
	const String name = "rendering/reflections/sky_reflections/accurate_rough_reflections";
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps->has_setting(name));

	CHECK(ps->get_setting(name).get_type() == Variant::BOOL);
	CHECK(bool(ps->property_get_revert(name)));
	CHECK(bool(ps->get_setting(name)));

	// The renderer reads the setting once at startup.
	List<PropertyInfo> properties;
	ps->get_property_list(&properties);
	bool found = false;
	for (const PropertyInfo &property : properties) {
		if (property.name == name) {
			found = true;
			CHECK((property.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) != 0);
		}
	}
	CHECK(found);
}

} // namespace TestSkyReflectionSettings
