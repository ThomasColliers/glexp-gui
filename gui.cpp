#include <iostream>
#include <vector>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glfw.h>
#include "berkelium/Berkelium.hpp"
#include "berkelium/Window.hpp"
#include "berkelium/WindowDelegate.hpp"

#include "Batch.h"
#include "ShaderManager.h"
#include "Frame.h"
#include "Frustum.h"
#include "TransformPipeline.h"
#include "MatrixStack.h"

#include "GLTextureWindow.h"

// TODO: Interaction (on arbitrary surfaces like a sphere)
//
//First create a texture where you encode u and v in say r channel and
//first half of g channel (12 bits) and for v on half of g and b
//channels.
//use this texture to draw you object (textured, but without mipmap and
//without anti-aliasing) in the back buffer.
//In order to know the texture coordinate under the mouse, just read the
//back buffer pixel under the mouse, decode the rgb in uv and you got
//it.
//the drawback of this method is you need to draw your scene twice. one
//with the scene, one only in the back buffer with the special texture.
//
// OR:
//
//The problem is best solved outside OGL. If you got your data set, you
//have triangles with vertex positions. Intersect a ray, find which
//triangle it hits and use the barycentric coordinate to compute any
//gradient aka. varying at the point of intersection.
//
//You need the projection matrix to map the pixel coordinate into view
//coordinate. Then you need the inverse of the modelview matrix to map
//the view coordinate into model coordinate. At this point the problem
//is pretty much solved one.
//
//If you can store the scene/dataset hierarchically the intersection
//computation should be be several orders of magnitude quicker than
//asking feedback from the driver in a form or other, including the
//already suggested techniques with the kind of datasets that the OGL
//typically renders in the first place. YMMV, in which case something
//more specific can be suggested.
//
// TODO: Render to overlay
// TODO: Javascript interaction
// TODO: Add window to switch models, that window will be in orthographic projection on top of everything else

int mouse_x, mouse_y;
int window_w, window_h;
GLTextureWindow* texture_window;
GLTextureWindow* second_window;
// geometry
gliby::Batch* quad;
// shader & texture stuff
gliby::ShaderManager* shaderManager;
GLuint shader;
GLuint uiTestShader;
GLint locMatrix;
GLint locTexture;
GLuint texture;
GLuint second_texture;
GLuint locUiTestMatrix;
// transformation stuff
gliby::Frame objectFrame;
gliby::Frame cameraFrame;
gliby::Frustum viewFrustum;
gliby::TransformPipeline transformPipeline;
gliby::MatrixStack modelViewMatrix;
gliby::MatrixStack projectionMatrix;
// additional framebuffers
GLuint uiTestBuffer;
GLuint uiTestRenderBuffers[3];

void setupContext(void){
    // clearing color
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    // depth testing
    glEnable(GL_DEPTH_TEST);
    // blendmode
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // create framebuffer for UI sampling
    glGenFramebuffers(1, &uiTestBuffer); 
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, uiTestBuffer);
    // 2 color buffers and a depth buffer
    glGenRenderbuffers(2, uiTestRenderBuffers);
    glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, window_w, window_h);
    glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, window_w, window_h);
    glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[2]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, window_w, window_h);
    // attach to fbo
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, uiTestRenderBuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, uiTestRenderBuffers[1]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, uiTestRenderBuffers[2]);
    // set default framebuffer back
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // setup the transform pipeline
    transformPipeline.setMatrixStacks(modelViewMatrix,projectionMatrix);
    viewFrustum.setPerspective(35.0f, float(window_w)/float(window_h),1.0f,500.0f);
    projectionMatrix.loadMatrix(viewFrustum.getProjectionMatrix());
    modelViewMatrix.loadIdentity();
    cameraFrame.moveForward(-3.0f);
    objectFrame.rotateWorld(degToRad(0.0f), 0.0f, 1.0f, 0.0f);

    // setup shaders
    std::vector<const char*>* searchPath = new std::vector<const char*>();
    searchPath->push_back("./shaders/");
    searchPath->push_back("/home/ego/projects/personal/gliby/shaders/");
    shaderManager = new gliby::ShaderManager(searchPath);
    gliby::ShaderAttribute attrs[] = {{0,"vVertex"},{3,"vTexCoord"}};
    shader = shaderManager->buildShaderPair("simple_perspective.vp","simple_perspective.fp",sizeof(attrs)/sizeof(gliby::ShaderAttribute),attrs);
    if(shader == 0) std::cerr << "Shader build failed..." << std::endl;
    locMatrix = glGetUniformLocation(shader, "mvpMatrix");
    locTexture = glGetUniformLocation(shader, "textureUnit");
    uiTestShader = shaderManager->buildShaderPair("ui_test.vp","ui_test.fp",sizeof(attrs)/sizeof(gliby::ShaderAttribute),attrs);
    if(uiTestShader == 0) std::cerr << "Shader build failed..." << std::endl;
    //locUiTestMatrix = glGetUniformLocation(uiTestShader, "mvpMatrix");

    // setup quad
    quad = new gliby::Batch();
    quad->begin(GL_TRIANGLE_FAN, 4, 1);
    float z = 0.0f;
    float width = 1.0f; float height = 1.0f;
    GLfloat verts[] = {
        -width/2, height/2, z,
        -width/2, -height/2, z,
        width/2, -height/2, z,
        width/2, height/2, z
    };
    GLfloat texcoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };
    GLfloat colors[] = {
        1.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };
    quad->copyVertexData3f(verts);
    quad->copyTexCoordData2f(texcoords,0);
    quad->copyColorData4f(colors);
    quad->end();

    // initialize berkelium
    if(!Berkelium::init(Berkelium::FileString::empty())){
        std::cout << "Failed to initialize Berkelium!" << std::endl;
    }
    // create a berkelium window
    glActiveTexture(GL_TEXTURE0);
    texture_window = new GLTextureWindow(600,600,false,false);
    texture = texture_window->texture();
    texture_window->window()->focus(); // geeft (apparent?) input focus
    texture_window->clear();
    std::string url("http://thomascolliers.com");
    texture_window->window()->navigateTo(url.data(),url.length());

    // create second window
    second_window = new GLTextureWindow(600,600,true,false);
    second_texture = second_window->texture();
    second_window->window()->focus();
    second_window->clear();
    //std::string url2("http://share.thomascolliers.com/pagetest/");
    std::string url2("http://google.be/");
    second_window->window()->navigateTo(url2.data(),url2.length());

    //texture = ilutGLLoadImage("texture.tga");
    /*glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    GLbyte bits[512*512*4];
    for(int i = 0; i < (512*512*4)/2; i++){
        bits[i] = rand() % 127; 
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);*/
    /*if(glfwLoadTexture2D("texture.tga",NULL)){
        std::cout << "Texture loaded" << std::endl;
    }else{
        std::cout << "Texture not loaded" << std::endl;
    }*/
}

void receiveInput(){
    /*if(glfwGetKey(GLFW_KEY_ESC) == GLFW_PRESS){
        glfwCloseWindow();
    }*/
    glfwGetMousePos(&mouse_x, &mouse_y);
}
void keyCallback(int id, int state){
    if(id == GLFW_KEY_ESC && state == GLFW_RELEASE){
        glfwCloseWindow();
    }
}
void mouseCallback(int id, int state){
    //std::cout << id << " " << state << std::endl;
}

void render(void){
    // clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    // update berkelium
    Berkelium::update();

    // set up camera
    cameraFrame.moveForward(3.0f);
    cameraFrame.rotateWorld(0.01f, 0.0f, 1.0f, 0.0f);
    cameraFrame.moveForward(-3.0f);
    Math3D::Matrix44f mCamera;
    cameraFrame.getCameraMatrix(mCamera);

    // draw quad
    glUseProgram(shader);
    // matrix
    modelViewMatrix.pushMatrix();
    modelViewMatrix.multMatrix(mCamera);
    glUniformMatrix4fv(locMatrix, 1, GL_FALSE, transformPipeline.getModelViewProjectionMatrix());
    // texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(locTexture, 0);
    //draw
    quad->draw();
    // pop matrix
    modelViewMatrix.popMatrix();

    // set up object transformation
    Math3D::Matrix44f mObject;
    objectFrame.getMatrix(mObject);

    // draw second quad
    modelViewMatrix.pushMatrix();
    modelViewMatrix.multMatrix(mCamera);
    modelViewMatrix.multMatrix(mObject);
    glUniformMatrix4fv(locMatrix, 1, GL_FALSE, transformPipeline.getModelViewProjectionMatrix());
    glBindTexture(GL_TEXTURE_2D, second_texture);
    quad->draw();
    modelViewMatrix.popMatrix();
}

void resize(int width, int height){
    window_w = width;
    window_h = height;
    // update gl viewport
    glViewport(0,0,window_w,window_h);
    // update render buffer sizes
    if(uiTestBuffer){
        glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, window_w, window_h);
        glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, window_w, window_h);
        glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[2]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, window_w, window_h);
    }
    // update projection matrix
    viewFrustum.setPerspective(35.0f, float(window_w)/float(window_h),1.0f,500.0f);
    projectionMatrix.loadMatrix(viewFrustum.getProjectionMatrix());
}

int main(void){
    // enable vsync with an env hint to the driver
    //putenv((char*) "__GL_SYNC_TO_VBLANK=1");
    
    // init glfw and window
    if(!glfwInit()){
        std::cerr << "GLFW init failed" << std::endl;
        return -1;
    }
    glfwSwapInterval(1);
    glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 8);
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 4);
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
    glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if(!glfwOpenWindow(800, 600, 8, 8, 8, 0, 24, 0, GLFW_WINDOW)){
        std::cerr << "GLFW window opening failed" << std::endl;
        return -1;
    }
    window_w = 800; window_h = 600;
    glfwSetKeyCallback(keyCallback);
    glfwSetMouseButtonCallback(mouseCallback);
    glfwSetWindowSizeCallback(resize);
    glfwSetWindowTitle("gltest");

    // init glew
    glewExperimental = GL_TRUE; // without this glGenVertexArrays() segfaults :/, should be fixed in GLEW 1.7
    GLenum err = glewInit();
    if(err != GLEW_OK){
        std::cerr << "Glew error: " << glewGetErrorString(err) << std::endl;
        return -1;
    }

    // setup context
    setupContext();

    // main loop
    while(glfwGetWindowParam(GLFW_OPENED)){
        receiveInput();
        render(); 
        glfwSwapBuffers();
    }

    Berkelium::destroy();
    glfwTerminate();

    return 0;
}
