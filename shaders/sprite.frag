#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;

void main() {
  vec4 texel = texture(spriteTexture, TexCoord);
  if (texel.a < 0.01) discard;
  FragColor = texel;
}
