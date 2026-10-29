#include "AppDelegate.h"
#include "mfm1.inc"
#include "mfm4.inc"
#include "mfm2.inc"
#include "mfm3.inc"

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
    director->setContentScaleFactor(2.0f);
    auto scene = Scene::create();
    scene->addChild(GameLayer::create());
    director->runWithScene(scene);
    return true;
}

void AppDelegate::applicationDidEnterBackground() { Director::getInstance()->stopAnimation(); AudioEngine::pauseAll(); }
void AppDelegate::applicationWillEnterForeground() { Director::getInstance()->startAnimation(); AudioEngine::resumeAll(); }
