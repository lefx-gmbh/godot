#[versions]

lines = "#define MODE_LINES";
triangles = "#define MODE_TRIANGLES";

#[vertex]

#version 450

#VERSION_DEFINES

#include "lm_common_inc.glsl"

layout(push_constant, std430) uniform Params {
	uint base_index;
	uint slice;
	vec2 uv_offset;
	bool debug;
	float blend;
	float edge_offset;
	uint pad;
}
params;

layout(location = 0) out vec3 uv_interp;

#ifdef MODE_LINES
layout(location = 1) out vec2 dst_edge_texel;
layout(location = 2) flat out vec2 dst_outward;
layout(location = 3) flat out vec2 src_step;
layout(location = 4) flat out float line_blend;

layout(set = 1, binding = 0) uniform texture2DArray src_color_tex;

const float SNAP_EPSILON = 0.01; // Texels. An edge on a texel center reads that texel itself.
const float VALID_ALPHA = 0.9; // The dilate pass marks the texels that it fills with a lower alpha.

// Returns the unit normal of the edge in texels. It points to the side of the opposite vertex, into the chart.
vec2 inward_normal(uvec2 p_edge, uint p_opposite, vec2 p_atlas_size) {
	vec2 from = vertices.data[p_edge.x].uv * p_atlas_size;
	vec2 edge = vertices.data[p_edge.y].uv * p_atlas_size - from;
	vec2 normal = vec2(-edge.y, edge.x) / max(length(edge), 1e-6);
	return dot(normal, vertices.data[p_opposite].uv * p_atlas_size - from) < 0.0 ? -normal : normal;
}

// Returns the first texel inside the chart at a point of the edge, with texel centers at integers. On a texel
// boundary, a bilinear read on the edge takes half of the texel outside the chart. Only the axis nearest to the
// edge normal is snapped, so on a diagonal edge, the read stays bilinear along the other axis.
vec2 first_inner_texel(vec2 p_uv, vec2 p_inward, vec2 p_atlas_size) {
	vec2 texel = p_uv * p_atlas_size - 0.5;
	if (abs(p_inward.x) >= abs(p_inward.y)) {
		texel.x = p_inward.x > 0.0 ? ceil(texel.x - SNAP_EPSILON) : floor(texel.x + SNAP_EPSILON);
	} else {
		texel.y = p_inward.y > 0.0 ? ceil(texel.y - SNAP_EPSILON) : floor(texel.y + SNAP_EPSILON);
	}
	return texel;
}

// Returns true if the gutter can copy this side of the seam. The triangle must be at least one texel high, and
// the first texel inside must be a valid texel at the middle of the edge. At the ends, a chart corner can put
// that texel just outside the chart, also on a large chart.
bool can_copy_from(uvec2 p_edge, uint p_opposite, vec2 p_atlas_size) {
	vec2 inward = inward_normal(p_edge, p_opposite, p_atlas_size);
	float height = dot(vertices.data[p_opposite].uv * p_atlas_size - vertices.data[p_edge.x].uv * p_atlas_size, inward);
	vec2 middle = (vertices.data[p_edge.x].uv + vertices.data[p_edge.y].uv) * 0.5;
	ivec2 texel = clamp(ivec2(round(first_inner_texel(middle, inward, p_atlas_size))), ivec2(0), ivec2(p_atlas_size) - 1);
	return height >= 1.0 - SNAP_EPSILON && texelFetch(sampler2DArray(src_color_tex, linear_sampler), ivec3(texel, params.slice), 0).a > VALID_ALPHA;
}
#endif

void main() {
#ifdef MODE_TRIANGLES
	uint triangle_idx = params.base_index + gl_VertexIndex / 3;
	uint triangle_subidx = gl_VertexIndex % 3;

	vec2 uv;
	if (triangle_subidx == 0) {
		uv = vertices.data[triangles.data[triangle_idx].indices.x].uv;
	} else if (triangle_subidx == 1) {
		uv = vertices.data[triangles.data[triangle_idx].indices.y].uv;
	} else {
		uv = vertices.data[triangles.data[triangle_idx].indices.z].uv;
	}

	uv_interp = vec3(uv, float(params.slice));
	gl_Position = vec4((uv + params.uv_offset) * 2.0 - 1.0, 0.0001, 1.0);
#endif

#ifdef MODE_LINES
	uint seam_idx = params.base_index + gl_VertexIndex / 4;
	uint seam_subidx = gl_VertexIndex % 4;

	uint src_idx;
	uint dst_idx;

	if (seam_subidx == 0) {
		src_idx = seams.data[seam_idx].b.x;
		dst_idx = seams.data[seam_idx].a.x;
	} else if (seam_subidx == 1) {
		src_idx = seams.data[seam_idx].b.y;
		dst_idx = seams.data[seam_idx].a.y;
	} else if (seam_subidx == 2) {
		src_idx = seams.data[seam_idx].a.x;
		dst_idx = seams.data[seam_idx].b.x;
	} else if (seam_subidx == 3) {
		src_idx = seams.data[seam_idx].a.y;
		dst_idx = seams.data[seam_idx].b.y;
	}

	// Vertices 0 and 1 draw the line of side a, vertices 2 and 3 draw the line of side b.
	bool dst_is_a = seam_subidx < 2;
	uvec2 dst_edge = dst_is_a ? seams.data[seam_idx].a : seams.data[seam_idx].b;
	uvec2 src_edge = dst_is_a ? seams.data[seam_idx].b : seams.data[seam_idx].a;
	uvec2 opposite = seams.data[seam_idx].opposite;
	vec2 atlas_size = vec2(bake_params.atlas_size);

	// Move the line out of the destination chart, along the edge normal.
	dst_edge_texel = vertices.data[dst_idx].uv * atlas_size;
	dst_outward = -inward_normal(dst_edge, dst_is_a ? opposite.x : opposite.y, atlas_size);
	vec2 dst_uv = (dst_edge_texel + dst_outward * params.edge_offset) / atlas_size;

	if (can_copy_from(seams.data[seam_idx].a, opposite.x, atlas_size) && can_copy_from(seams.data[seam_idx].b, opposite.y, atlas_size)) {
		// Copy the other side into the gutter. Read it at its first texel inside the chart, not on the edge. That
		// texel can hold a raster sample of the edge instead of a copy of the chart. The side of the chart comes
		// from the opposite vertex, so the two charts can have different UV winding.
		vec2 src_inward = inward_normal(src_edge, dst_is_a ? opposite.y : opposite.x, atlas_size);
		// Deeper gutter texels read deeper along the edge normal. The destination counts its depth along its own
		// edge normal, so this keeps the position along the seam, also when the charts have different orientations.
		src_step = src_inward;
		uv_interp = vec3((first_inner_texel(vertices.data[src_idx].uv, src_inward, atlas_size) + 0.5) / atlas_size, float(params.slice));
		// The copy does not change the texels on the edge.
		line_blend = params.edge_offset > 0.0 ? 1.0 : 0.0;
	} else {
		// A chart thinner than a texel, or a source texel outside the chart: the copy can read a wrong value.
		// Then average both sides on the edge and in the gutter. Read the other side on its edge.
		src_step = vec2(0.0);
		uv_interp = vec3(vertices.data[src_idx].uv, float(params.slice));
		line_blend = 0.5;
	}
	gl_Position = vec4(dst_uv * 2.0 - 1.0, 0.0001, 1.0);
#endif
}

#[fragment]

#version 450

#VERSION_DEFINES

#include "lm_common_inc.glsl"

layout(push_constant, std430) uniform Params {
	uint base_index;
	uint slice;
	vec2 uv_offset;
	bool debug;
	float blend;
	float edge_offset;
	uint pad;
}
params;

layout(location = 0) in vec3 uv_interp;

#ifdef MODE_LINES
layout(location = 1) in vec2 dst_edge_texel;
layout(location = 2) flat in vec2 dst_outward;
layout(location = 3) flat in vec2 src_step;
layout(location = 4) flat in float line_blend;

const float SNAP_EPSILON = 0.01; // Texels. Same as in the vertex shader.
const float VALID_ALPHA = 0.9; // Same as in the vertex shader.
#endif

layout(location = 0) out vec4 dst_color;

layout(set = 1, binding = 0) uniform texture2DArray src_color_tex;

void main() {
	if (params.debug) {
#ifdef MODE_TRIANGLES
		dst_color = vec4(1, 0, 1, 1);
#else
		dst_color = vec4(1, 1, 0, 1);
#endif
	} else {
#ifdef MODE_LINES
		if (line_blend == 0.0) {
			discard;
		}
		// The k-th gutter texel outside the edge reads the k-th texel inside the source chart. With supersampling,
		// the gutter texels of one final texel then hold the texels of one final texel on the other side.
		float edge_distance = dot(gl_FragCoord.xy - dst_edge_texel, dst_outward);
		float steps = max(ceil(edge_distance - SNAP_EPSILON) - 1.0, 0.0);
		vec3 uv = vec3(uv_interp.xy + src_step * steps / vec2(bake_params.atlas_size), uv_interp.z);
		float blend = line_blend;
		if (blend < 1.0 && texelFetch(sampler2DArray(src_color_tex, linear_sampler), ivec3(gl_FragCoord.xy, uv_interp.z), 0).a < VALID_ALPHA) {
			// The source texture is a copy of this one. A texel that the dilate pass filled takes the other side.
			blend = 1.0;
		}
#else
		vec3 uv = uv_interp;
		float blend = params.blend;
#endif
		vec4 src_color = textureLod(sampler2DArray(src_color_tex, linear_sampler), uv, 0.0);
		dst_color = vec4(src_color.rgb, blend); //mix
	}
}
