#version 430

precision mediump float;

smooth in vec4 vVertCol;

out vec4 gl_FragColor;

void main(void){
    gl_FragColor = vVertCol;
}
