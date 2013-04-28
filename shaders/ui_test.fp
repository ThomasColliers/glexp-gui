#version 430

precision mediump float;

uniform int objectIndex;

smooth in vec2 vTex;

out vec4 gl_FragColor;

void main(void){
    vec4 c = vec4(0.0,0.0,0.0,1.0);
    // put the objectIndex in the R channel
    c.r = float(objectIndex)/256.0;
    // and the texture coordinates in G and B channel
    c.g = vTex[0];
    c.b = vTex[1];
    gl_FragColor = c;
}
