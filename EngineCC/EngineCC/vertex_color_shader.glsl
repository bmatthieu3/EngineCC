#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_texcoords;

uniform mat4 modelview;
uniform mat4 model;
uniform mat4 projection;

out vec4 vert_color;
out vec3 vert_texcoords;
out vec2 frag_coord;

void main() {
	gl_Position = projection * modelview * vec4(in_position, 1.0f);
	vert_color = in_color;
	vert_texcoords = in_texcoords;
	frag_coord = (model * vec4(in_position, 1)).xz;
}