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
#include "Actor.h"

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
// shader & texture stuff
gliby::ShaderManager* shaderManager;
GLuint shader;
GLuint uiTestShader;
// transformation stuff
gliby::Frame cameraFrame;
gliby::Frustum viewFrustum;
gliby::TransformPipeline transformPipeline;
gliby::MatrixStack modelViewMatrix;
gliby::MatrixStack projectionMatrix;
// additional framebuffers
GLuint uiTestBuffer;
GLuint uiTestRenderBuffers[3];
// actors
gliby::Actor* planes[2];

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

    // setup shaders
    std::vector<const char*>* searchPath = new std::vector<const char*>();
    searchPath->push_back("./shaders/");
    searchPath->push_back("/home/ego/projects/personal/gliby/shaders/");
    shaderManager = new gliby::ShaderManager(searchPath);
    gliby::ShaderAttribute attrs[] = {{0,"vVertex"},{3,"vTexCoord"}};
    shader = shaderManager->buildShaderPair("simple_perspective.vp","simple_perspective.fp",sizeof(attrs)/sizeof(gliby::ShaderAttribute),attrs);
    uiTestShader = shaderManager->buildShaderPair("ui_test.vp","ui_test.fp",sizeof(attrs)/sizeof(gliby::ShaderAttribute),attrs);

    // setup quad
    gliby::Batch* quad = new gliby::Batch();
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
    GLTextureWindow* texture_window = new GLTextureWindow(600,600,false,false);
    //texture = texture_window->texture();
    texture_window->window()->focus(); // geeft (apparent?) input focus
    texture_window->clear();
    std::string url("http://thomascolliers.com");
    texture_window->window()->navigateTo(url.data(),url.length());
    // create second window
    GLTextureWindow* second_window = new GLTextureWindow(600,600,true,false);
    //second_texture = second_window->texture();
    second_window->window()->focus();
    second_window->clear();
    std::string url2("http://google.be/");
    second_window->window()->navigateTo(url2.data(),url2.length());

    planes[0] = new gliby::Actor(quad,texture_window->texture()); 
    planes[1] = new gliby::Actor(quad,second_window->texture());
    planes[0]->getFrame().rotateWorld(degToRad(180.0f), 0.0f, 1.0f, 0.0f);
    planes[1]->getFrame().rotateWorld(degToRad(0.0f), 0.0f, 1.0f, 0.0f);
}

void receiveInput(){
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

void draw(GLuint sh){
    // ui draw pass
    glUseProgram(sh);
    for(int i = 0; i < 2; i++){
        // setup matrix
        modelViewMatrix.pushMatrix();
        Math3D::Matrix44f mObject;
        planes[i]->getFrame().getMatrix(mObject);
        modelViewMatrix.multMatrix(mObject);
        // load correct texture
        if(planes[i]->getTexture()) glBindTexture(GL_TEXTURE_2D, planes[i]->getTexture());
        // set uniforms
        GLint locMVP = glGetUniformLocation(sh,"mvpMatrix");
        if(locMVP != -1) glUniformMatrix4fv(locMVP, 1, GL_FALSE, transformPipeline.getModelViewProjectionMatrix());
        GLint locTexture = glGetUniformLocation(sh,"textureUnit");
        if(locTexture != -1) glUniform1i(locTexture, 0);
        GLint locIndex = glGetUniformLocation(sh,"objectIndex");
        if(locIndex != -1) glUniform1i(locIndex, i);
        // draw
        planes[i]->getGeometry().draw();
        // clear matrix
        modelViewMatrix.popMatrix();
    }
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
    modelViewMatrix.pushMatrix();
    modelViewMatrix.multMatrix(mCamera);

    //draw(uiTestShader);
    draw(shader);

    // pop off camera transformations
    modelViewMatrix.popMatrix();

    /*cameraFrame.moveForward(3.0f);
    cameraFrame.rotateWorld(0.01f, 0.0f, 1.0f, 0.0f);
    cameraFrame.moveForward(-3.0f);
    Math3D::Matrix44f mCamera;
    cameraFrame.getCameraMatrix(mCamera);

    // set up mvp matrices for both panes
    modelViewMatrix.pushMatrix();
    modelViewMatrix.multMatrix(mCamera);
    const Math3D::Matrix44f& mvpPlane1 = transformPipeline.getModelViewProjectionMatrix();
    Math3D::Matrix44f mObject;
    objectFrame.getMatrix(mObject);
    modelViewMatrix.multMatrix(mObject);
    const Math3D::Matrix44f& mvpPlane2 = transformPipeline.getModelViewProjectionMatrix();
    modelViewMatrix.popMatrix();

    // ui draw pass
    // plane 1
    glUseProgram(uiTestShader);
    glUniformMatrix4fv(locUiTestMatrix, 1, GL_FALSE, mvpPlane1);
    glUniform1i(locUiTestIndex, 0);
    quad->draw();
    // plane 2
    glUniformMatrix4fv(locUiTestMatrix, 1, GL_FALSE, mvpPlane2);
    glUniform1i(locUiTestIndex, 1);
    quad->draw();*/


    // texture
    /*glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(locTexture, 0);*/
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
