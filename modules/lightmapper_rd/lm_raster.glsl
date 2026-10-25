#[vertex]

#version 450

#VERSION_DEFINES

#include "lm_common_inc.glsl"

layout(location = 0) out vec3 vertex_interp;
layout(location = 1) out vec3 normal_interp;
layout(location = 2) out vec2 uv_interp;
layout(location = 3) out vec3 barycentric;
layout(location = 4) flat out uvec3 vertex_indices;
layout(location = 5) flat out vec3 face_normal;
layout(location = 6) flat out uint fragment_action;

layout(push_constant, std430) uniform Params {
	vec2 atlas_size;
	vec2 uv_offset;
	vec3 to_cell_size;
	uint base_triangle;
	vec3 to_cell_offset;
	float bias;
	ivec3 grid_size;
	uint pad2;
}
params;

void main() {
	uint triangle_idx = params.base_triangle + gl_VertexIndex / 3;
	uint triangle_subidx = gl_VertexIndex % 3;

	vertex_indices = triangles.data[triangle_idx].indices;

	uint vertex_idx;
	if (triangle_subidx == 0) {
		vertex_idx = vertex_indices.x;
		barycentric = vec3(1, 0, 0);
	} else if (triangle_subidx == 1) {
		vertex_idx = vertex_indices.y;
		barycentric = vec3(0, 1, 0);
	} else {
		vertex_idx = vertex_indices.z;
		barycentric = vec3(0, 0, 1);
	}

	vertex_interp = vertices.data[vertex_idx].position;
	uv_interp = vertices.data[vertex_idx].uv;
	normal_interp = vec3(vertices.data[vertex_idx].normal_xy, vertices.data[vertex_idx].normal_z);

	face_normal = -normalize(cross((vertices.data[vertex_indices.x].position - vertices.data[vertex_indices.y].position), (vertices.data[vertex_indices.x].position - vertices.data[vertex_indices.z].position)));

	{
		const float FLAT_THRESHOLD = 0.99;
		const vec3 norm_a = vec3(vertices.data[vertex_indices.x].normal_xy, vertices.data[vertex_indices.x].normal_z);
		const vec3 norm_b = vec3(vertices.data[vertex_indices.y].normal_xy, vertices.data[vertex_indices.y].normal_z);
		const vec3 norm_c = vec3(vertices.data[vertex_indices.z].normal_xy, vertices.data[vertex_indices.z].normal_z);
		fragment_action = (dot(norm_a, norm_b) < FLAT_THRESHOLD || dot(norm_a, norm_c) < FLAT_THRESHOLD || dot(norm_b, norm_c) < FLAT_THRESHOLD) ? FA_SMOOTHEN_POSITION : FA_NONE;
	}

	gl_Position = vec4((uv_interp + params.uv_offset) * 2.0 - 1.0, 0.0001, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

#include "lm_common_inc.glsl"

layout(push_constant, std430) uniform Params {
	vec2 atlas_size;
	vec2 uv_offset;
	vec3 to_cell_size;
	uint base_triangle;
	vec3 to_cell_offset;
	float bias;
	ivec3 grid_size;
	uint pad2;
}
params;

layout(location = 0) in vec3 vertex_interp;
layout(location = 1) in vec3 normal_interp;
layout(location = 2) in vec2 uv_interp;
layout(location = 3) in vec3 barycentric;
layout(location = 4) in flat uvec3 vertex_indices;
layout(location = 5) in flat vec3 face_normal;
layout(location = 6) in flat uint fragment_action;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 unocclude;

void main() {
	vec3 pos_a = vertices.data[vertex_indices.x].position;
	vec3 pos_b = vertices.data[vertex_indices.y].position;
	vec3 pos_c = vertices.data[vertex_indices.z].position;

	vec3 vertex_pos = vertex_interp;
	vec3 weights = barycentric;
	{
		// Rays start at bias from the sample and ignore hits nearer than bias. So a sample nearer than
		// 2 * bias to an edge can send rays past a wall that meets the edge. Keep a margin a little
		// larger than that, but not more than a third of the smallest triangle height. That third is not
		// more than the inradius, so a point at the margin from all edges always exists.
		const float EDGE_MARGIN_BIASES = 2.1; // 2 * bias, plus some slack.
		vec3 n = cross(pos_b - pos_a, pos_c - pos_a);
		vec3 edges[3] = vec3[](pos_c - pos_b, pos_a - pos_c, pos_b - pos_a);
		vec3 heights = length(n) / vec3(length(edges[0]), length(edges[1]), length(edges[2]));
		vec3 inward[3] = vec3[](normalize(cross(n, edges[0])), normalize(cross(n, edges[1])), normalize(cross(n, edges[2])));
		if (all(greaterThan(heights, vec3(0.0)))) {
			float margin = min(EDGE_MARGIN_BIASES * params.bias, min(heights.x, min(heights.y, heights.z)) / 3.0);
			// The weight of a vertex is the distance to the opposite edge divided by the height.
			vec3 min_weights = margin / heights;
			// The weights add up to 1, and the minimum weights add up to 1 or less. So at most two edges are near.
			bvec3 near = lessThan(weights, min_weights);
			if (int(near.x) + int(near.y) + int(near.z) == 1) {
				// Move straight away from the edge. A move along the edge would shift the sample on thin triangles.
				int i = near.x ? 0 : (near.y ? 1 : 2);
				vec3 move = inward[i] * (min_weights[i] - weights[i]) * heights[i];
				for (int j = 0; j < 3; j++) {
					weights[j] += dot(move, inward[j]) / heights[j];
				}
				// Near an acute vertex, the move can bring the sample nearer than the margin to a second edge.
				near = lessThan(weights, min_weights);
				near[i] = true;
			}
			if (int(near.x) + int(near.y) + int(near.z) >= 2) {
				// Near a vertex, use the point at the margin from both edges. The far vertex takes the rest of the weight.
				weights = mix(vec3(0.0), min_weights, near);
				weights += (1.0 - dot(weights, vec3(1.0))) * vec3(not(near));
				// Rounding can set all three edges near at the center of a small triangle. Keep the sum at 1.
				weights /= dot(weights, vec3(1.0));
			}
			if (any(near)) {
				vertex_pos = pos_a * weights.x + pos_b * weights.y + pos_c * weights.z;
			}
		}
	}

	if (fragment_action == FA_SMOOTHEN_POSITION) {
		// smooth out vertex position by interpolating its projection in the 3 normal planes (normal plane is created by vertex pos and normal)
		// because we don't want to interpolate inwards, normals found pointing inwards are pushed out.
		vec3 center = (pos_a + pos_b + pos_c) * 0.3333333;
		vec3 norm_a = vec3(vertices.data[vertex_indices.x].normal_xy, vertices.data[vertex_indices.x].normal_z);
		vec3 norm_b = vec3(vertices.data[vertex_indices.y].normal_xy, vertices.data[vertex_indices.y].normal_z);
		vec3 norm_c = vec3(vertices.data[vertex_indices.z].normal_xy, vertices.data[vertex_indices.z].normal_z);

		{
			vec3 dir_a = normalize(pos_a - center);
			float d_a = dot(dir_a, norm_a);
			if (d_a < 0) {
				//pointing inwards
				norm_a = normalize(norm_a - dir_a * d_a);
			}
		}
		{
			vec3 dir_b = normalize(pos_b - center);
			float d_b = dot(dir_b, norm_b);
			if (d_b < 0) {
				//pointing inwards
				norm_b = normalize(norm_b - dir_b * d_b);
			}
		}
		{
			vec3 dir_c = normalize(pos_c - center);
			float d_c = dot(dir_c, norm_c);
			if (d_c < 0) {
				//pointing inwards
				norm_c = normalize(norm_c - dir_c * d_c);
			}
		}

		float d_a = dot(norm_a, pos_a);
		float d_b = dot(norm_b, pos_b);
		float d_c = dot(norm_c, pos_c);

		vec3 proj_a = vertex_pos - norm_a * (dot(norm_a, vertex_pos) - d_a);
		vec3 proj_b = vertex_pos - norm_b * (dot(norm_b, vertex_pos) - d_b);
		vec3 proj_c = vertex_pos - norm_c * (dot(norm_c, vertex_pos) - d_c);

		// Use the weights of the sample after the edge margin, so that they agree with vertex_pos.
		vec3 smooth_position = proj_a * weights.x + proj_b * weights.y + proj_c * weights.z;

		if (dot(face_normal, smooth_position) > dot(face_normal, vertex_pos)) { //only project outwards
			vertex_pos = smooth_position;
		}
	}

	{
		// unocclusion technique based on:
		// https://ndotl.wordpress.com/2018/08/29/baking-artifact-free-lightmaps/

		/* compute texel size */
		vec3 delta_uv = max(abs(dFdx(vertex_interp)), abs(dFdy(vertex_interp)));
		float texel_size = max(delta_uv.x, max(delta_uv.y, delta_uv.z));
		texel_size *= sqrt(2.0); //expand to unit box edge length (again, worst case)

		unocclude.xyz = face_normal;
		unocclude.w = texel_size;

		//continued on lm_compute.glsl
	}

	position = vec4(vertex_pos, 1.0);
	normal = vec4(normalize(normal_interp), 1.0);
}
