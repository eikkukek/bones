#pragma once

namespace shaders {

	class World {
	public:

		static constexpr const char* pbr_draw_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outPosition;
layout(location = 2) out vec3 outNormal;

layout(set = 0, binding = 0) uniform CameraMatrices {
	mat4 c_Projection;
	mat4 c_View;
} camera_matrices;

layout(push_constant) uniform PushConstant {
	layout(offset = 0) 
	mat4 c_Transform;
	mat4 c_NormalMatrix;
} pc;

void main() {

	const vec3 modelPos = vec3(inPosition.x, -inPosition.y, inPosition.z);

	outUV = inUV;

	outNormal = normalize(vec3(vec4(inNormal, 0.0f) * pc.c_NormalMatrix));

	outPosition = vec3(pc.c_Transform * vec4(modelPos, 1.0f));

	gl_Position = camera_matrices.c_Projection * camera_matrices.c_View * pc.c_Transform * vec4(modelPos, 1.0f);

}
			)";

		static constexpr const char* pbr_draw_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outDiffuseColor;
layout(location = 1) out vec4 outPositionAndMetallic;
layout(location = 2) out vec4 outNormalAndRougness;

layout(set = 1, binding = 0) uniform sampler2D diffuse_map;

void main() {
	outDiffuseColor = texture(diffuse_map, inUV);
	outPositionAndMetallic = vec4(inPosition, 1.0f);
	outNormalAndRougness = vec4(inNormal, 1.0f);
}
		)";

		static constexpr const char * ud_draw_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(push_constant) uniform PushConstant {
	layout(offset = 0)
	mat4 c_LightView;
	mat4 c_Transform;
} pc;

void main() {
	gl_Position = pc.c_LightView * pc.c_Transform * vec4(inPosition.x, -inPosition.y, inPosition.z, 1.0f);
}
		)";

		static constexpr const char* pbr_render_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main() {
	outUV = inUV;
	gl_Position = vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
		)";

		static constexpr const char* pbr_render_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D diffuse_colors;
layout(set = 0, binding = 1) uniform sampler2D position_and_metallic;
layout(set = 0, binding = 2) uniform sampler2D normal_and_roughness;

layout(set = 1, binding = 0) uniform sampler2D directional_light_shadow_map;

layout(set = 1, binding = 1) uniform DirectionalLight {
	mat4 c_ViewSpaceMatrix;
	vec3 c_Direction;
	vec3 c_Color;
} directional_light;

bool IsInShadowDirLight(vec4 lightViewPos, float bias) {

	const vec4 shadowMapCoords = lightViewPos / lightViewPos.w;

	if (shadowMapCoords.z > -1.0f && shadowMapCoords.z < 1.0f) {
		float dist = texture(directional_light_shadow_map, shadowMapCoords.st * 0.5f + 0.5f).r;
		return shadowMapCoords.w > 0.0f && dist < shadowMapCoords.z - bias;
	}

	return false;
}

float GetBias(float lnDot) {
	return 0.005f - 0.0015f * lnDot;
}

void main() {

	const vec4 modelPosAndMetal = texture(position_and_metallic, inUV);

	const vec3 pos = modelPosAndMetal.xyz;
	const vec3 normal = vec3(texture(normal_and_roughness, inUV));	

	vec4 lightViewPos
		= directional_light.c_ViewSpaceMatrix * vec4(modelPosAndMetal.xyz, 1.0f);

	vec3 lightDir = directional_light.c_Direction;

	float dnDot = max(dot(normal, lightDir), 0.0f);

	const float diffMul = IsInShadowDirLight(lightViewPos, GetBias(dnDot)) ? 0.0f : dnDot;

	const vec3 diffuse = diffMul * directional_light.c_Color;

	vec3 color = (vec3(0.2f, 0.2f, 0.2f) + diffuse) * vec3(texture(diffuse_colors, inUV));
	float gamma = 2.2f;
	color = pow(color, vec3(1.0f / gamma));

	outColor = vec4(color, 1.0f);
}
		)";

		static constexpr const char* debug_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(set = 0, binding = 0) uniform CameraMatrices {
	mat4 c_Projection;
	mat4 c_View;
} camera_matrices;

layout(push_constant) uniform PushConstant {
	layout(offset = 0) 
	mat4 c_Transform;
} pc;

void main() {
	gl_Position = camera_matrices.c_Projection * camera_matrices.c_View * pc.c_Transform * vec4(inPosition.x, -inPosition.y, inPosition.z, 1.0f);
}
		)";

		static constexpr const char* debug_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant {
	layout(offset = 64)
	vec4 c_Color;
	vec2 c_CursorPos;
	int c_NoID;
	uint c_ID;
} pc;

layout(std140, set = 1, binding = 0) buffer MouseHitBuffer {
	float m_Depth;
	uint m_ID;
} hitBuffer;

void main() {
	vec4 fCoord = gl_FragCoord;
	if (int(fCoord.x) == pc.c_CursorPos.x && int(fCoord.y) == pc.c_CursorPos.y) {
		if (pc.c_NoID != 0 && fCoord.z < hitBuffer.m_Depth) {
			hitBuffer.m_Depth = fCoord.z;
			hitBuffer.m_ID = pc.c_ID;
		}
	}
	outColor = pc.c_Color;
}
		)";
	};

	class Editor {
	public:

		static constexpr const char* sdf_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outRayOrigin;
layout(location = 2) out vec3 outRayDirection;

layout(push_constant) uniform PushConstant {
	layout(offset = 0)
	mat4 c_InverseCameraMatrix;
	float c_CameraNear;
	float c_CameraFar;
} pc;

layout(set = 0, binding = 0) uniform Transform {
	mat4 c_RectTransform;
} transform;

void main() {

	const float near = pc.c_CameraNear;
	const float far = pc.c_CameraFar;

	vec2 pos = vec2(inPosition.x, -inPosition.y);
	outRayOrigin = (pc.c_InverseCameraMatrix * vec4(pos, -1.0f, 1.0f) * near).xyz;
	outRayDirection = (pc.c_InverseCameraMatrix * vec4(pos * (far - near), far + near, far - near)).xyz;

	outUV = inUV;

	gl_Position = transform.c_RectTransform * vec4(inPosition.x, -inPosition.y, inPosition.z, 1.0f);
}
		)";

		static constexpr const char* sdf_pipeline_fragment_shader = R"(
#version 450

#define PI 3.14159265359

#define STATE_ROTATORS 1U

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inRayOrigin;
layout(location = 2) in vec3 inRayDirection;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform RotatorInfo {
	mat4 c_InverseTransformX;
	mat4 c_InverseTransformY;
	mat4 c_InverseTransformZ;
	vec4 c_ColorX;
	vec4 c_ColorY;
	vec4 c_ColorZ;
	float c_Radius;
	float c_Thickness;	
} rotator_info;

layout(std140, set = 2, binding = 0) buffer MouseHitBuffer {
	uint m_Hit;
} hitBuffer;

layout(push_constant) uniform PushConstant {
	layout(offset = 80)
	uvec2 c_Resolution;
	uvec2 c_MousePosition;
	uint c_State;
} pc;

float SdTorus(vec3 pos, float r, float t) {
	vec2 q = vec2(length(pos.xz) - r, pos.y);
	return length(q) - t;
}

const float tmax = 100.0f;
vec4 color;
uint hit = 0;

float Map(vec3 pos) {

	float min = 1000.0f;
	
	if ((pc.c_State & STATE_ROTATORS) > 0) {

		float x = SdTorus((rotator_info.c_InverseTransformX * vec4(pos, 1.0f)).xyz, rotator_info.c_Radius, rotator_info.c_Thickness);
		float y = SdTorus((rotator_info.c_InverseTransformY * vec4(pos, 1.0f)).xyz, rotator_info.c_Radius, rotator_info.c_Thickness);
		float z = SdTorus((rotator_info.c_InverseTransformZ * vec4(pos, 1.0f)).xyz, rotator_info.c_Radius, rotator_info.c_Thickness);

		color = rotator_info.c_ColorX;
		hit = 1;
		min = x;

		if (y < min) {
			min = y;
			color = rotator_info.c_ColorY;
			hit = 2;
		}
		if (z < min) {
			min = z;
			color = rotator_info.c_ColorZ;
			hit = 3;
		}
	}
	return min;
}

float PhaseSchlick(vec3 w, vec3 wp, float g) {
	float k = 1.55f * g - 0.55f * g * g * g;
	float kCosTheta = k * dot(w, wp);
	return 1.0f / (4.0f * PI) *
		(1.0f - k * k) / ((1.0f - kCosTheta) * 1.0f - kCosTheta);
}

void main() {

	const vec3 ro = inRayOrigin;
	const vec3 rd = normalize(inRayDirection);	

	float t = 0.0f;
	for (int i = 0; i < 256; i++) {
		vec3 pos = ro + rd * t;
		float h = Map(pos);
		if (h < 0.0001f || t > tmax) {
			break;
		}
		t += h;
	}

	if (t < tmax) {
		outColor = color;
		if (pc.c_MousePosition == uvec2(pc.c_Resolution * inUV)) {
			hitBuffer.m_Hit = hit;
		}
	}
	else {
		outColor = vec4(0.0f);
	}
}
		)";
	};
}
