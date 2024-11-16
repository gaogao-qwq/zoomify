#version 330

// Input vertex attributes from vertex shader
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

uniform float opacity;
uniform vec2 center;
uniform float radius;
uniform int textureWidth;
uniform int textureHeight;

void main() {
    vec2 texSize = vec2(float(textureWidth), float(textureHeight));
    vec2 texCenter = fragTexCoord * texSize;
    texCenter -= center;
    float dist = length(texCenter);
    vec4 color = fragColor;

	if (dist > radius) {
		float gray = 0.05;
		color.x = gray;
		color.y = gray;
		color.z = gray;
		color.w = opacity;
	}

    finalColor = color;
}
