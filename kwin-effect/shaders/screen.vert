in vec2 vertex;
in vec2 texCoord;
uniform mat4 modelViewProjectionMatrix;
out vec2 uv;
void main()
{
    uv = texCoord;
    gl_Position = modelViewProjectionMatrix * vec4(vertex, 0.0, 1.0);
}
