#version 430

precision mediump float;

uniform int objectIndex;

smooth in vec2 vTex;

out vec4 gl_FragData[];

vec3 valueToRGB(float val){
    vec3 c;
    c.b = floor(val / 256.0 / 256.0);
    c.g = floor((val - c.b * 256.0 * 256.0) / 256.0);
    c.r = floor(val - c.b * 256.0 * 256.0 - c.g * 256.0);
    return c / 256.0;
}

void main(void){
    // let's pack the index value into an rgb value
    vec4 indexColor;
    indexColor.rgb = valueToRGB(float(objectIndex)).rgb;
    indexColor.a = 1.0;
    // pack the texture coordinates in an rgb value
    vec4 texColor = vec4(0.0,0.0,0.0,1.0);
    texColor[0] = vTex[0];
    texColor[1] = vTex[1];
    // TODO: for better accuracy I should use the b channel as well

    gl_FragData[0] = indexColor;
    gl_FragData[1] = texColor;
}
