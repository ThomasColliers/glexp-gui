#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <boost/filesystem.hpp>
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
#include "TriangleBatch.h"
#include "GeometryFactory.h"

#include "GLTextureWindow.h"

// TODO: Sometimes the vertex buffer seems corrupt at initialisation
// TODO: Accuracy problems (need higher resolution color buffer?)

const int WINDOW_RESOLUTION = 600;

int mouse_x, mouse_y;
int window_w, window_h;
std::string current_path;
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
GLuint uiTestRenderBuffers[2];
GLuint pixelBuffers[2];
int pbo_index;
// texture windows
GLTextureWindow* texture_window;
GLTextureWindow* second_window;
GLTextureWindow* over_window;
// actors
gliby::Actor* objs[3];
// rotate camera?
bool rotateCamera;

void stopCameraRotation(){
    rotateCamera = false; 
}

void setupContext(void){
    // clearing color
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
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
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, window_w, window_h);
    glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, window_w, window_h);
    // attach to fbo
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, uiTestRenderBuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, uiTestRenderBuffers[1]);
    // set default framebuffer back
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // create 2 PBO's to manage asynchronous pixel reads
    glGenBuffers(2, pixelBuffers);
    for(int i = 0; i < 2; i++){
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelBuffers[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, 4, NULL, GL_STREAM_READ); // TODO: why not GL_DYNAMIC_READ?
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    pbo_index = 0;

    // init some vars
    mouse_x = 0; mouse_y = 0;
    rotateCamera = true;

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
    // setup sphere
    gliby::TriangleBatch& sphereBatch = gliby::GeometryFactory::sphere(0.2f, 20, 20);

    // initialize berkelium
    if(!Berkelium::init(Berkelium::FileString::empty())){
        std::cout << "Failed to initialize Berkelium!" << std::endl;
    }
    // create a berkelium window
    glActiveTexture(GL_TEXTURE0);
    texture_window = new GLTextureWindow(WINDOW_RESOLUTION,WINDOW_RESOLUTION,false,false);
    texture_window->window()->focus(); // geeft (apparent?) input focus
    texture_window->clear();
    std::string url("http://thomascolliers.com");
    texture_window->window()->navigateTo(url.data(),url.length());

    // create second window
    second_window = new GLTextureWindow(WINDOW_RESOLUTION,WINDOW_RESOLUTION,false,true);
    second_window->window()->focus();
    // register callback handler
    second_window->window()->addBindOnStartLoading(Berkelium::WideString::point_to(L"stopCameraRotation"),Berkelium::Script::Variant::bindFunction(Berkelium::WideString::point_to(L"stopCameraRotation"),false));
    CallbackHandler* handler = new CallbackHandler({Berkelium::WideString::point_to(L"stopCameraRotation"),stopCameraRotation});
    second_window->registerCallback(handler);
    // load local file
    second_window->clear();
    std::string localurl = current_path;
    localurl.insert(0, "file://");
    localurl.append("/example.html");
    second_window->window()->navigateTo(localurl.data(),localurl.length());
    over_window = NULL;

    gliby::Actor* planes[2];
    planes[0] = new gliby::Actor(quad,texture_window->texture()); 
    planes[1] = new gliby::Actor(quad,second_window->texture());
    planes[0]->getFrame().rotateWorld(degToRad(180.0f), 0.0f, 1.0f, 0.0f);
    planes[1]->getFrame().rotateWorld(degToRad(0.0f), 0.0f, 1.0f, 0.0f);
    gliby::Actor* sphere = new gliby::Actor(&sphereBatch,texture_window->texture());
    sphere->getFrame().moveForward(0.5);
    sphere->getFrame().rotateWorld(degToRad(-90.0f), 1.0f, 0.0f, 0.0f);
    objs[0] = planes[0];
    objs[1] = planes[1];
    objs[2] = sphere;
}

void receiveInput(){
    glfwGetMousePos(&mouse_x, &mouse_y);
}
void keyCallback(int id, int state){
    if(id == GLFW_KEY_ESC && state == GLFW_RELEASE){
        glfwCloseWindow();
    }
}
void charCallback(int character, int action){
    if(over_window){
        //std::cout << character << " " << action << std::endl;
        wchar_t c[2];
        c[0] = character;
        c[1] = 0;
        over_window->window()->textEvent(c,1);
    }
}
void mouseCallback(int id, int state){
    if(id == 0 && over_window){
        over_window->window()->mouseButton(0, state);
    }
}

void draw(GLuint sh){
    // ui draw pass
    glUseProgram(sh);
    for(int i = 0; i < 3; i++){
        // setup matrix
        modelViewMatrix.pushMatrix();
        Math3D::Matrix44f mObject;
        objs[i]->getFrame().getMatrix(mObject);
        modelViewMatrix.multMatrix(mObject);
        // load correct texture
        if(objs[i]->getTexture()) glBindTexture(GL_TEXTURE_2D, objs[i]->getTexture());
        // set uniforms
        GLint locMVP = glGetUniformLocation(sh,"mvpMatrix");
        if(locMVP != -1) glUniformMatrix4fv(locMVP, 1, GL_FALSE, transformPipeline.getModelViewProjectionMatrix());
        GLint locTexture = glGetUniformLocation(sh,"textureUnit");
        if(locTexture != -1) glUniform1i(locTexture, 0);
        GLint locIndex = glGetUniformLocation(sh,"objectIndex");
        if(locIndex != -1) glUniform1i(locIndex, i);
        // draw
        objs[i]->getGeometry().draw();
        // clear matrix
        modelViewMatrix.popMatrix();
    }
}

void render(void){
    // show framerate
    static int frameCount = 0;
    static long currentSecond = 0;
    frameCount++;
    if((int)glfwGetTime() > currentSecond){
        std::cout << "Framerate: " << frameCount << std::endl;
        frameCount = 0;
        currentSecond = (int)glfwGetTime();
    }

    // update berkelium
    Berkelium::update();

    // set up camera
    if(rotateCamera){
        cameraFrame.moveForward(3.0f);
        cameraFrame.rotateWorld(0.01f, 0.0f, 1.0f, 0.0f);
        cameraFrame.moveForward(-3.0f);
    }
    Math3D::Matrix44f mCamera;
    cameraFrame.getCameraMatrix(mCamera);
    modelViewMatrix.pushMatrix();
    modelViewMatrix.multMatrix(mCamera);

    // draw with test ui shader
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, uiTestBuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, uiTestBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw(uiTestShader);
    // switch pbo index
    pbo_index = (pbo_index + 1) % 2;
    int next_index = (pbo_index + 1) % 2;
    // read pixels from framebuffer to PBO
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelBuffers[pbo_index]);
    glReadPixels(mouse_x, window_h-mouse_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    // map the pbo to process data by CPU
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelBuffers[next_index]);
    GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER,GL_READ_ONLY);
    if(ptr){
        if((int)ptr[3] > 0){
            int over_index = (int)ptr[0]; 
            if(over_index == 1) over_window = second_window;
            else over_window = texture_window;
            // retrieve texture coordinates
            float mousePos_x = (float)ptr[1]/256.0f;
            float mousePos_y = (float)ptr[2]/256.0f;
            over_window->window()->mouseMoved(mousePos_x*WINDOW_RESOLUTION,mousePos_y*WINDOW_RESOLUTION);
        }else{
            over_window = NULL;
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }else{
        over_window = NULL;
    }
    // back to conventional pixel operation
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    
    // normal drawing
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    draw(shader);

    // pop off camera transformations
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
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, window_w, window_h);
        glBindRenderbuffer(GL_RENDERBUFFER, uiTestRenderBuffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, window_w, window_h);
    }
    // update projection matrix
    viewFrustum.setPerspective(35.0f, float(window_w)/float(window_h),1.0f,500.0f);
    projectionMatrix.loadMatrix(viewFrustum.getProjectionMatrix());
}

int main(int argc, char **argv){
    // enable vsync with an env hint to the driver
    //putenv((char*) "__GL_SYNC_TO_VBLANK=1");
    
    // get current path

    current_path = boost::filesystem::system_complete(argv[0]).parent_path().parent_path().string();

    // init glfw and window
    if(!glfwInit()){
        std::cerr << "GLFW init failed" << std::endl;
        return -1;
    }
    glfwSwapInterval(1); // set to 0 to disable vsync, 1 to enable
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
    glfwSetCharCallback(charCallback);
    glfwSetMouseButtonCallback(mouseCallback);
    glfwSetWindowSizeCallback(resize);
    glfwSetWindowTitle("gltest");

    // init glew
    glewExperimental = GL_TRUE; // without this glGenVertexArrays() segfaults :/, should be fixed in GLEW 1.7?
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
