#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GL/glfw.h>
#include <IL/ilut.h>
#include "berkelium/Berkelium.hpp"
#include "berkelium/Window.hpp"
#include "berkelium/WindowDelegate.hpp"

#include "Batch.h"
#include "ShaderManager.h"

#include "GLTextureWindow.h"

// TODO: Set up quad and shader to use texture
// TODO: Load in Berkelium texture, update each frame
// TODO: Transparency testing
// TODO: Multiple windows
// TODO: Interaction
// TODO: Interaction on arbitrary surfaces (like a sphere)
// TODO: Render to overlay
// TODO: Mipmapping performance?
// TODO: Javascript interaction

int mouse_x, mouse_y;
int window_w, window_h;
GLTextureWindow* texture_window;
ShaderManager* shaderManager;
Batch* quad;
GLuint shader;
GLuint locMatrix;
GLuint locTextureUnit;

void setupContext(void){
    // clearing color
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    // depth testing
    glEnable(GL_DEPTH_TEST);
    // culling
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    // set up maxed out anisotropic filtering
    GLfloat largest;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest);
    // blendmode
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // setup shader
    std::vector<const char*>* searchPath = new std::vector<const char*>();
    searchPath->push_back("./shaders/");
    searchPath->push_back("/home/ego/projects/personal/gliby/shaders/");
    shaderManager = new ShaderManager(searchPath);
    ShaderAttribute attrs[] = {{0,"vVertex"},{3,"vTexCoord"}};
    shader = shaderManager->buildShaderPair("texture.vp","texture.fp",sizeof(attrs)/sizeof(ShaderAttribute),attrs);
    if(shader == 0){
        std::cerr << "Shader build failed..." << std::endl;
    }
    locMatrix = glGetUniformLocation(shader, "mvpMatrix");
    locTextureUnit = glGetUniformLocation(shader, "textureUnit");

    // setup quad
    quad = new Batch();
    quad->begin(GL_TRIANGLE_FAN, 4, 1);
    float x = 0.1; float y = 0.1;
    float width = 0.8; float height = 0.8;
    GLfloat verts[] = {
        x, y, 0.0f,
        x, y-height, 0.0f,
        x + width, y - height, 0.0f,
        x + width, y, 0.0f
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

    // setup texture
    ilInit();
    iluInit();
    ilutRenderer(ILUT_OPENGL);
    ILuint imageName;
    ilGenImages(1, &imageName);
    ilBindImage(imageName);
    ilLoadImage("texture.tga");

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
    // use program
    glUseProgram(shader);
    /*GLfloat color[] = {1.0f, 0.0f, 0.0f, 1.0f};
    glUniform4fv(locColor,1,color);*/
    quad->draw();
}

void resize(int width, int height){
    window_w = width;
    window_h = height;
    glViewport(0,0,window_w,window_h);
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

    // initialize berkelium
    if(!Berkelium::init(Berkelium::FileString::empty())){
        std::cout << "Failed to initialize Berkelium!" << std::endl;
    }
    // create a berkelium window
    texture_window = new GLTextureWindow(200,100,false,false);
    texture_window->window()->focus(); // TODO: check wat dit doet?
    // load page into the window
    texture_window->clear();
    std::string url("http://thomascolliers.com");
    texture_window->window()->navigateTo(url.data(),url.length());

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
