// Esqueleto: carrega a primeira cena CocosBuilder. Ajuste o nome do .ccbi para o menu real do jogo.
#include "AppDelegate.h"
#include "cocos2d.h"
#include "extensions/cocos-ext.h"
#include "editor-support/cocosbuilder/CocosBuilder.h"
USING_NS_CC;
using namespace cocosbuilder;

bool AppDelegate::applicationDidFinishLaunching() {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview) { glview = GLViewImpl::create("Mutant Fridge Mayhem"); director->setOpenGLView(glview); }
    glview->setDesignResolutionSize(480, 320, ResolutionPolicy::FIXED_HEIGHT); // original iPhone, landscape
    FileUtils::getInstance()->addSearchPath("data"); FileUtils::getInstance()->addSearchPath("textures");
    NodeLoaderLibrary* lib = NodeLoaderLibrary::newDefaultNodeLoaderLibrary();
    CCBReader* reader = new CCBReader(lib);
    Node* menu = reader->readNodeGraphFromFile("GameMainMenu.ccbi");
    auto scene = Scene::create(); if (menu) scene->addChild(menu);
    director->runWithScene(scene);
    return true;
}
