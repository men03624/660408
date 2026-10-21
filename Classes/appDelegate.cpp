#include "AppDelegate.h"
#include "cocos2d.h"
#if __has_include("editor-support/cocosbuilder/CocosBuilder.h")
#include "editor-support/cocosbuilder/CocosBuilder.h"
#define HAVE_CCB 1
#endif
USING_NS_CC;

AppDelegate::AppDelegate() {}
AppDelegate::~AppDelegate() {}

void AppDelegate::initGLContextAttrs() {
    GLContextAttrs glContextAttrs = {8, 8, 8, 8, 24, 8, 0};
    GLView::setGLContextAttrs(glContextAttrs);
}

bool AppDelegate::applicationDidFinishLaunching() {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview) { glview = GLViewImpl::create("Mutant Fridge Mayhem"); director->setOpenGLView(glview); }
    glview->setDesignResolutionSize(480, 320, ResolutionPolicy::FIXED_HEIGHT);
    FileUtils::getInstance()->addSearchPath("data");
    FileUtils::getInstance()->addSearchPath("textures");
    auto scene = Scene::create();
    bool ok = false;
#ifdef HAVE_CCB
    {
        using namespace cocosbuilder;
        NodeLoaderLibrary* lib = NodeLoaderLibrary::newDefaultNodeLoaderLibrary();
        CCBReader* reader = new CCBReader(lib);
        Node* menu = reader->readNodeGraphFromFile("GameMainMenu.ccbi");
        if (menu) { scene->addChild(menu); ok = true; }
    }
#endif
    if (!ok) { scene->addChild(Label::createWithSystemFont("Mutant Fridge Mayhem", "Arial", 24)); }
    director->runWithScene(scene);
    return true;
}

void AppDelegate::applicationDidEnterBackground() { Director::getInstance()->stopAnimation(); }
void AppDelegate::applicationWillEnterForeground() { Director::getInstance()->startAnimation(); }
