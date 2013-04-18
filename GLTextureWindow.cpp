#include "GLTextureWindow.h"
#include <iostream>

GLTextureWindow::GLTextureWindow(unsigned int w, unsigned int h, bool transp, bool verb):
    width(w),height(h),needs_full_refresh(true),verbose(verb){

    // generate a texture
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // generate scroll buffer
    scroll_buffer = new char[width*(height+1)*4];

    // create window
    Berkelium::Context *context = Berkelium::Context::create();
    bk_window = Berkelium::Window::create(context);
    delete context;
    bk_window->setDelegate(this);
    bk_window->resize(width, height);
    bk_window->setTransparent(transp);
}
GLTextureWindow::~GLTextureWindow(void){
    delete scroll_buffer;
    delete bk_window;
    glDeleteTextures(1, &texture_id); // will cause problems if texture is still being used
}

Berkelium::Window* GLTextureWindow::window(void) const {
    return bk_window;
}
GLuint GLTextureWindow::texture(void) const{
    return texture_id;
}

void GLTextureWindow::clear(void){
    unsigned char black = 0;
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, &black);
    needs_full_refresh = true;
}

void GLTextureWindow::onPaint(Berkelium::Window* win, const unsigned char* bitmap_in, const Berkelium::Rect &bitmap_rect,
    size_t num_copy_rects, const Berkelium::Rect* copy_rects, int dx, int dy, const Berkelium::Rect &scroll_rect){
    
    // handle painting
    //std::cout << "Paint" << std::endl;

    needs_full_refresh = false;
}


void GLTextureWindow::onAddressBarChanged(Berkelium::Window* win, Berkelium::URLString newURL){
    if(verbose) std::cout << "BK: Address bar URL changed to " << newURL << std::endl;
}
void GLTextureWindow::onStartLoading(Berkelium::Window* win, Berkelium::URLString newURL){
    if(verbose) std::cout << "BK: Starting loading of " << newURL << std::endl;
}
void GLTextureWindow::onLoad(Berkelium::Window* win){
    if(verbose) std::cout << "BK: Loading" << std::endl;
}
void GLTextureWindow::onCrashedWorker(Berkelium::Window* win){
    if(verbose) std::cerr << "BK: Worker crashed!" << std::endl;
}
void GLTextureWindow::onCrashedPlugin(Berkelium::Window* win, Berkelium::WideString pluginName){
    if(verbose) std::wcerr << "BK: Plugin crashed: " << pluginName << std::endl;
}
void GLTextureWindow::onProvisionalLoadError(Berkelium::Window* win, Berkelium::URLString url, int errorCode, bool isMainFrame){
    if(verbose){
        std::cerr << "BK: Provisional load error " << url << ": " << errorCode;
        if(isMainFrame) std::wcerr << " (main frame)";
        std::cerr << std::endl;
    }
}
void GLTextureWindow::onConsoleMessage(Berkelium::Window* win, Berkelium::WideString message, Berkelium::WideString sourceID, int line_no){
    if(verbose) std::wcout << "BK: Console message: " << message << " from " << sourceID << " line " << line_no << std::endl;
}
void GLTextureWindow::onScriptAlert(Berkelium::Window* win, Berkelium::WideString message, Berkelium::WideString defaultValue, Berkelium::URLString url, int flags, bool &success, Berkelium::WideString &value){
    if(verbose) std::wcout << "BK: Script alert: " << message << std::endl;
}
void GLTextureWindow::onNavigationRequested(Berkelium::Window* win, Berkelium::URLString newURL, Berkelium::URLString referrer, bool isNewWindow, bool &cancelDefaultAction){
    if(verbose) std::cout << "BK: Navigation requested " << newURL << " by " << referrer << (isNewWindow ? " (new window)" : " (same window)") << std::endl;
}
void GLTextureWindow::onLoadingStateChanged(Berkelium::Window* win, bool isLoading){
    if(verbose) std::cout << "BK: Loading state changed: " << (isLoading ? "loading" : "stopped") << std::endl;
}
void GLTextureWindow::onTitleChanged(Berkelium::Window* win, Berkelium::WideString title){
    if(verbose) std::wcout << "BK: Title changed to " << title << std::endl;
}
void GLTextureWindow::onTooltipCHanged(Berkelium::Window* win, Berkelium::WideString text){
    if(verbose) std::wcout << "BK: Tooltip changed to " << text << std::endl;
}
void GLTextureWindow::onCrashed(Berkelium::Window* win){
    if(verbose) std::cerr << "BK: Crashed!" << std::endl;
}
void GLTextureWindow::onUnresponsive(Berkelium::Window* win){
    if(verbose) std::cerr << "BK: Unresponsive" << std::endl;
}
void GLTextureWindow::onCreatedWindow(Berkelium::Window* win, Berkelium::Window* newWin, const Berkelium::Rect &initialRect){
    if(verbose) std::cout << "BK: Created window" << std::endl;
    if(initialRect.mWidth < 1 || initialRect.mHeight < 1){
        newWin->resize(width,height);
    }
    newWin->setDelegate(this);
}
void GLTextureWindow::onWidgetCreated(Berkelium::Window* win, Berkelium::Widget* widget, int zIndex){
    if(verbose) std::cout << "BK: Widget created " << widget << " index " << zIndex << std::endl;
}
void GLTextureWindow::onWidgetResize(Berkelium::Window* win, Berkelium::Widget* widget, int w, int h){
    if(verbose) std::cout << "BK: Widget resize " << widget << " " << w << "x" << h << std::endl;
}
void GLTextureWindow::onWidgetMove(Berkelium::Window* win, Berkelium::Widget* widget, int x, int y){
    if(verbose) std::cout << "BK: Widget move " << widget << " " << x << "x" << y << std::endl;
}
void GLTextureWindow::onShowContextMenu(Berkelium::Window* win, const Berkelium::ContextMenuEventArgs& args){
    if(verbose){
        std::cout << "BK: Context menu at " << args.mouseX << "," << args.mouseY << " type: ";
        switch(args.mediaType){
            case Berkelium::ContextMenuEventArgs::MediaTypeImage:
                std::cout << "image";
                break;
            case Berkelium::ContextMenuEventArgs::MediaTypeVideo:
                std::cout << "video";
                break;
            case Berkelium::ContextMenuEventArgs::MediaTypeAudio:
                std::cout << "audio";
                break;
            default:
                std::cout << "other";
                break;
        }
        if(args.isEditable || args.editFlags){
            std::cout << " (";
            if(args.isEditable) std::cout << "editable; ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanUndo) std::cout << "Undo, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanRedo) std::cout << "Redo, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanCut) std::cout << "Cut, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanCopy) std::cout << "Copy, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanPaste) std::cout << "Paste, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanDelete) std::cout << "Delete, ";
            if(args.editFlags & Berkelium::ContextMenuEventArgs::CanSelectAll) std::cout << "Select All, ";
        }
        std::cout << ")";
        std::cout << std::endl;
        if(args.linkUrl.length()) std::cout << "  Link URL: " << args.linkUrl << std::endl;
        if(args.srcUrl.length()) std::cout << "  Source URL: " << args.srcUrl << std::endl;
        if(args.pageUrl.length()) std::cout << "  Page URL: " << args.pageUrl << std::endl;
        if(args.frameUrl.length()) std::cout << "  Frame URL: " << args.frameUrl << std::endl;
        if(args.selectedText.length()) std::wcout << "  Selected Text: " << args.selectedText << std::endl;
    }
}

void GLTextureWindow::onJavascriptCallback(Berkelium::Window* win, void* replyMsg, Berkelium::URLString url, Berkelium::WideString funcName, Berkelium::Script::Variant *args, size_t numArgs){
    if(verbose){
        std::cout << "BK: Javascript callback at URL " << url << ", " << (replyMsg ? "synchronous" : "async") << std::endl;
        std::cout << "  Function name: " << funcName << std::endl;
        for(size_t i = 0; i < numArgs; i++){
            Berkelium::WideString jsonString = Berkelium::Script::toJSON(args[i]);
            std::wcout << "  Argument " << i << ": ";
            if(args[i].type() == Berkelium::Script::Variant::JSSTRING){
                std::wcout << "(string) " << args[i].toString() << std::endl;
            }else{
                std::wcout << jsonString << std::endl;
            }
            Berkelium::Script::toJSON_free(jsonString);
        }
        if(replyMsg){
            win->synchronousScriptReturn(replyMsg, numArgs ? args[0] : Berkelium::Script::Variant());
        }
    }
}

void GLTextureWindow::onRunFileChooser(Berkelium::Window* win, int mode, Berkelium::WideString title, Berkelium::FileString defaultFile){
    if(verbose){
        std::wcout << "BK: Run file chooser type " << mode << ", title " << title << ":" << std::endl;
        std::wcout << defaultFile << std::endl;
    }
    win->filesSelected(NULL);
}

void GLTextureWindow::onExternalHost(Berkelium::Window* win, Berkelium::WideString message, Berkelium::URLString origin, Berkelium::URLString target){
    if(verbose){
        std::cout << "BK: External host at URL from " << origin << " to " << target << ": " << message << std::endl;
    }
}