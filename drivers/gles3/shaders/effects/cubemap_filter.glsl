/* clang-format off */
#[modes]

mode_default =
mode_copy = #define MODE_DIRECT_WRITE
mode_irradiance = #define MODE_IRRADIANCE

#[specializations]

#[vertex]

layout(location = 0) in highp vec2 vertex_attrib;
/* clang-format on */

out highp vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib;
	gl_Position = vec4(uv_interp, 0.0, 1.0);
}

/* clang-format off */
#[fragment]


#define M_PI 3.14159265359

uniform samplerCube source_cube; //texunit:0

/* clang-format on */

uniform int face_id;

#ifdef MODE_IRRADIANCE
// The cosine convolution reads every texel of the source mip level source_lod.
// source_size is its edge length in texels.
uniform float source_lod;
uniform int source_size;
#endif
#if !defined(MODE_DIRECT_WRITE) && !defined(MODE_IRRADIANCE)
uniform uint sample_count;
uniform vec4 sample_directions_mip[MAX_SAMPLE_COUNT];
uniform float weight;
#endif
#ifndef MODE_DIRECT_WRITE
// 1 for linear source data (the sky radiance), 0 for sRGB encoded data (a reflection probe)
uniform uint linear_data;
#endif

in highp vec2 uv_interp;

layout(location = 0) out vec4 frag_color;

#define M_PI 3.14159265359

// Don't include tonemap_inc.glsl because all we want is these functions, we don't want the uniforms
// This approximation expects non-negative input; negative input is undefined behavior.
vec3 linear_to_srgb(vec3 color) {
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

// This approximation expects non-negative input; negative input behaves poorly.
vec3 srgb_to_linear(vec3 color) {
	return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

vec3 texelCoordToVec(vec2 uv, int faceID) {
	mat3 faceUvVectors[6];

	// -x
	faceUvVectors[1][0] = vec3(0.0, 0.0, 1.0); // u -> +z
	faceUvVectors[1][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[1][2] = vec3(-1.0, 0.0, 0.0); // -x face

	// +x
	faceUvVectors[0][0] = vec3(0.0, 0.0, -1.0); // u -> -z
	faceUvVectors[0][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[0][2] = vec3(1.0, 0.0, 0.0); // +x face

	// -y
	faceUvVectors[3][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[3][1] = vec3(0.0, 0.0, -1.0); // v -> -z
	faceUvVectors[3][2] = vec3(0.0, -1.0, 0.0); // -y face

	// +y
	faceUvVectors[2][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[2][1] = vec3(0.0, 0.0, 1.0); // v -> +z
	faceUvVectors[2][2] = vec3(0.0, 1.0, 0.0); // +y face

	// -z
	faceUvVectors[5][0] = vec3(-1.0, 0.0, 0.0); // u -> -x
	faceUvVectors[5][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[5][2] = vec3(0.0, 0.0, -1.0); // -z face

	// +z
	faceUvVectors[4][0] = vec3(1.0, 0.0, 0.0); // u -> +x
	faceUvVectors[4][1] = vec3(0.0, -1.0, 0.0); // v -> -y
	faceUvVectors[4][2] = vec3(0.0, 0.0, 1.0); // +z face

	// out = u * s_faceUv[0] + v * s_faceUv[1] + s_faceUv[2].
	vec3 result = (faceUvVectors[faceID][0] * uv.x) + (faceUvVectors[faceID][1] * uv.y) + faceUvVectors[faceID][2];
	return normalize(result);
}

void main() {
	vec3 color = vec3(0.0);
	vec2 uv = uv_interp;
	vec3 N = texelCoordToVec(uv, face_id);

#ifdef MODE_DIRECT_WRITE
	frag_color = vec4(textureLod(source_cube, N, 0.0).rgb, 1.0);
#elif defined(MODE_IRRADIANCE)
	vec3 sum = vec3(0.0);
	float total = 0.0;
	float texel = 2.0 / float(source_size);
	for (int f = 0; f < 6; f++) {
		// texelCoordToVec() returns normalize(face_axis + u * u_axis + v * v_axis). Get the three axes once per face.
		vec3 face_axis = texelCoordToVec(vec2(0.0), f);
		vec3 u_axis = texelCoordToVec(vec2(1.0, 0.0), f) * sqrt(2.0) - face_axis;
		vec3 v_axis = texelCoordToVec(vec2(0.0, 1.0), f) * sqrt(2.0) - face_axis;
		for (int y = 0; y < source_size; y++) {
			for (int x = 0; x < source_size; x++) {
				vec2 st = (vec2(float(x), float(y)) + 0.5) * texel - 1.0;
				float t = 1.0 + dot(st, st);
				float solid_angle = inversesqrt(t * t * t); // Relative to the texel area.
				total += solid_angle;
				vec3 L = (face_axis + st.x * u_axis + st.y * v_axis) * inversesqrt(t);
				float NdotL = dot(N, L);
				if (NdotL > 0.0) {
					vec3 val = textureLod(source_cube, L, source_lod).rgb;
					if (linear_data == 0u) {
						val = srgb_to_linear(val);
					}
					sum += val * (NdotL * solid_angle);
				}
			}
		}
	}
	// All texels together cover 4 pi. So 4 / total converts the sum to irradiance / pi.
	sum *= 4.0 / total;
	if (linear_data == 0u) {
		sum = linear_to_srgb(sum);
	}
	frag_color = vec4(sum, 1.0);
#else

	vec4 sum = vec4(0.0);
	vec3 UpVector = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	mat3 T;
	T[0] = normalize(cross(UpVector, N));
	T[1] = cross(N, T[0]);
	T[2] = N;

	for (uint sample_num = 0u; sample_num < sample_count; sample_num++) {
		vec4 sample_direction_mip = sample_directions_mip[sample_num];
		vec3 L = T * sample_direction_mip.xyz;
		vec3 val = textureLod(source_cube, L, sample_direction_mip.w).rgb;
		// Mix using linear
		if (linear_data == 0u) {
			val = srgb_to_linear(val);
		}
		sum.rgb += val * sample_direction_mip.z;
	}

	sum /= weight;

	if (linear_data == 0u) {
		sum.rgb = linear_to_srgb(sum.rgb);
	}
	frag_color = vec4(sum.rgb, 1.0);
#endif
}
