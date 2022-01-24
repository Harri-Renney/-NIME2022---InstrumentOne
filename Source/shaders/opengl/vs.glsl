#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTex;

out vec2 tex_c;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    //Pass texture coordinate to fragment shader//
    //vec2 newTex = vec2(aTex.x, 1.0 - aTex.y)
	tex_c       = vec2(aTex.x, 1.0 - aTex.y);   
}