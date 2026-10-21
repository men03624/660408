#include "AppDelegate.h"
#include "cocos2d.h"
USING_NS_CC;

AppDelegate::AppDelegate() {}
AppDelegate::~AppDelegate() {}

void AppDelegate::initGLContextAttrs() {
    GLContextAttrs glContextAttrs = {8, 8, 8, 8, 24, 8, 0};
    GLView::setGLContextAttrs(glContextAttrs);
}

static Vector<SpriteFrame*> framesOf(const char* fmt, int n) {
    Vector<SpriteFrame*> v;
    auto cache = SpriteFrameCache::getInstance();
    for (int i = 0; i < n; i++) {
        auto f = cache->getSpriteFrameByName(StringUtils::format(fmt, i));
        if (f) v.pushBack(f);
    }
    return v;
}

bool AppDelegate::applicationDidFinishLaunching() {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview) { glview = GLViewImpl::create("Mutant Fridge Mayhem"); director->setOpenGLView(glview); }
    glview->setDesignResolutionSize(480, 320, ResolutionPolicy::FIXED_HEIGHT);

    auto fu = FileUtils::getInstance();
    auto scene = Scene::create();
    auto vs = director->getVisibleSize();
    auto org = director->getVisibleOrigin();
    auto cache = SpriteFrameCache::getInstance();

    std::string dbg = StringUtils::format(
        "HelloWorld.png: %d\nplist em data/: %d\npng em textures/: %d\nplist na raiz: %d\nccbi em data/: %d\nGumball plist em data/: %d",
        (int)fu->isFileExist("HelloWorld.png"),
        (int)fu->isFileExist("data/mfm-game-stage1-bg.plist"),
        (int)fu->isFileExist("textures/mfm-game-stage1-bg.png"),
        (int)fu->isFileExist("mfm-game-stage1-bg.plist"),
        (int)fu->isFileExist("data/GameMainMenu.ccbi"),
        (int)fu->isFileExist("data/mfm-game-gumball-sprites.plist"));

    cache->addSpriteFramesWithFile("data/mfm-game-stage1-bg.plist", "textures/mfm-game-stage1-bg.png");
    auto bgf = cache->getSpriteFrameByName("stages_c1_mainbg.png");
    if (bgf) {
        auto bg = Sprite::createWithSpriteFrame(bgf);
        bg->setScale(vs.height / bg->getContentSize().height);
        bg->setPosition(org.x + vs.width / 2, org.y + vs.height / 2);
        scene->addChild(bg);
        dbg += "\nFUNDO: OK";
    } else { dbg += "\nFUNDO: FALHOU"; }

    cache->addSpriteFramesWithFile("data/mfm-game-gumball-sprites.plist", "textures/mfm-game-gumball-sprites.png");
    auto body = framesOf("char_gumball_idle_%03d.png", 8);
    auto arms = framesOf("char_gumball_idle_arms_%03d.png", 8);
    if (!body.empty()) {
        auto g = Sprite::createWithSpriteFrame(body.at(0));
        g->setPosition(org.x + vs.width / 2, org.y + vs.height * 0.4f);
        g->runAction(RepeatForever::create(Animate::create(Animation::createWithSpriteFrames(body, 0.07f))));
        scene->addChild(g, 1);
        if (!arms.empty()) {
            auto a = Sprite::createWithSpriteFrame(arms.at(0));
            a->setPosition(g->getPosition());
            a->runAction(RepeatForever::create(Animate::create(Animation::createWithSpriteFrames(arms, 0.07f))));
            scene->addChild(a, 2);
        }
        dbg += StringUtils::format("\nGUMBALL: OK (%d quadros)", (int)body.size());
    } else { dbg += "\nGUMBALL: FALHOU"; }

    auto label = Label::createWithSystemFont(dbg, "Arial", 11);
    label->setAnchorPoint(Vec2(0, 1));
    label->setPosition(org.x + 8, org.y + vs.height - 6);
    scene->addChild(label, 10);

    director->runWithScene(scene);
    return true;
}

void AppDelegate::applicationDidEnterBackground() { Director::getInstance()->stopAnimation(); }
void AppDelegate::applicationWillEnterForeground() { Director::getInstance()->startAnimation(); }
