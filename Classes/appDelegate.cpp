#include "AppDelegate.h"
#include "cocos2d.h"
#include "audio/include/AudioEngine.h"
#include <vector>
#include <algorithm>
#include <cmath>
USING_NS_CC;
namespace cocos2d { namespace experimental {} }
using namespace cocos2d::experimental;

typedef std::vector<SpriteFrame*> Frames;

static void sfx(const std::string& f, float v = .7f) { AudioEngine::play2d(f, false, v); }
static SpriteFrame* frm(const std::string& n) { return SpriteFrameCache::getInstance()->getSpriteFrameByName(n); }
static Frames seq(const char* fmt, int a, int b) {
    Frames v;
    for (int i = a; i <= b; i++) { auto f = frm(StringUtils::format(fmt, i)); if (f) v.push_back(f); }
    return v;
}
static SpriteFrame* pick(const Frames& v, float t, bool loop) {
    if (v.empty()) return nullptr;
    int n = (int)v.size(), i = (int)(t * 14);
    i = loop ? i % n : std::min(n - 1, i);
    return v[i];
}
static void setF(Sprite* s, SpriteFrame* f) {
    if (f) { s->setSpriteFrame(f); s->setVisible(true); } else s->setVisible(false);
}

struct Enemy {
    Node* n; Sprite *legs, *armL, *body, *head, *armR;
    float x, y, t, hp, hurt, sp;
    int st, f; bool did;   // st: 0 corre, 1 ataca, 2 morto
};

class GameLayer : public Layer {
public:
    CREATE_FUNC(GameLayer);
    bool init() override;
    void update(float dt) override;
private:
    void attack(bool charged);
    void spawnEnemy();
    void resetGame();
    void render();
    float cx = 0, xmin = 0, xmax = 0;
    Size vs; Vec2 org;
    Node* pn = nullptr; Sprite *pBody = nullptr, *pL = nullptr, *pR = nullptr;
    float px = 200, py = 70, pt = 0, pinv = 0, plastAtk = -9, T = 0, spawnT = 1, deadT = 0;
    int pf = 1, pst = 0, cb = 0, hp = 100, score = 0, best = 0, wave = 1;
    bool phit = false, over = false, chg = false;
    Frames idle, idleA, run, runL, runR, ch[3], death, legs, atkL, atkH;
    std::vector<Enemy> E;
    Vec2 joyO, joyV; int joyId = -1;
    int atkId = -1; float atkStart = 0;
    DrawNode* hud = nullptr; Label *lbl = nullptr, *lblOver = nullptr;
};

bool GameLayer::init() {
    if (!Layer::init()) return false;
    auto d = Director::getInstance();
    vs = d->getVisibleSize(); org = d->getVisibleOrigin();
    cx = org.x + vs.width / 2;
    auto cache = SpriteFrameCache::getInstance();
    cache->addSpriteFramesWithFile("data/mfm-game-stage1-bg.plist", "textures/mfm-game-stage1-bg.png");
    cache->addSpriteFramesWithFile("data/mfm-game-gumball-sprites.plist", "textures/mfm-game-gumball-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-npcbreadman-sprites.plist", "textures/mfm-game-npcbreadman-sprites.png");

    auto bgf = frm("stages_c1_mainbg.png");
    if (bgf) {
        auto bg = Sprite::createWithSpriteFrame(bgf);
        Size bs = bg->getContentSize();
        float s = std::max(vs.width / bs.width, vs.height / bs.height);
        bg->setScale(s);
        bg->setAnchorPoint(Vec2(0.5f, 0.f));
        bg->setPosition(cx, org.y);
        addChild(bg, -5000);
    }
    xmin = org.x + 40; xmax = org.x + vs.width - 40;
    px = cx;

    idle = seq("char_gumball_idle_%03d.png", 0, 7);
    idleA = seq("char_gumball_idle_arms_%03d.png", 0, 7);
    run = seq("char_gumball_run_%03d.png", 0, 9);
    runL = seq("char_gumball_run_larm_%03d.png", 0, 9);
    runR = seq("char_gumball_run_rarm_%03d.png", 0, 9);
    ch[0] = seq("char_gumball_chop1_%03d.png", 0, 3);
    ch[1] = seq("char_gumball_chop2_%03d.png", 0, 3);
    ch[2] = seq("char_gumball_chop3_%03d.png", 0, 7);
    death = seq("char_gumball_death_%03d.png", 0, 6);
    legs = seq("npc_breadman_run_legs_%03d.png", 0, 8);
    atkH = seq("npc_breadman_attack_headarms_%03d.png", 0, 5);

    pn = Node::create(); pn->setCascadeOpacityEnabled(true); pn->setCascadeColorEnabled(true);
    pL = Sprite::create(); pBody = Sprite::create(); pR = Sprite::create();
    pn->addChild(pL); pn->addChild(pBody); pn->addChild(pR);
    addChild(pn);

    hud = DrawNode::create(); addChild(hud, 5000);
    lbl = Label::createWithSystemFont("", "Arial", 16);
    lbl->setAnchorPoint(Vec2(1, 1)); lbl->setPosition(org.x + vs.width - 10, org.y + vs.height - 8);
    addChild(lbl, 5001);
    lblOver = Label::createWithSystemFont("", "Arial", 22);
    lblOver->setPosition(cx, org.y + vs.height / 2);
    addChild(lblOver, 5002);

    auto l = EventListenerTouchAllAtOnce::create();
    l->onTouchesBegan = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) {
            Vec2 p = t->getLocation();
            if (p.x > cx) { if (atkId < 0) { atkId = t->getID(); atkStart = T; } }
            else if (joyId < 0) { joyId = t->getID(); joyO = p; joyV = Vec2::ZERO; }
        }
    };
    l->onTouchesMoved = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) if (t->getID() == joyId) {
            Vec2 v = (t->getLocation() - joyO) / 40.f;
            if (v.length() > 1.f) v.normalize();
            joyV = v;
        }
    };
    l->onTouchesEnded = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) {
            if (t->getID() == joyId) { joyId = -1; joyV = Vec2::ZERO; }
            if (t->getID() == atkId) { float hold = T - atkStart; atkId = -1; attack(hold >= .8f); }
        }
    };
    l->onTouchesCancelled = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) {
            if (t->getID() == joyId) { joyId = -1; joyV = Vec2::ZERO; }
            if (t->getID() == atkId) atkId = -1;
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(l, this);
    resetGame();
    AudioEngine::play2d("audio/music_chapter1.mp3", true, 0.4f);
    scheduleUpdate();
    return true;
}

void GameLayer::resetGame() {
    for (auto& e : E) e.n->removeFromParent();
    E.clear();
    px = cx; py = 70; pf = 1; pst = 0; pt = 0; hp = 100; score = 0; wave = 1;
    pinv = 0; plastAtk = -9; cb = 0; spawnT = 1; over = false; chg = false;
}

void GameLayer::attack(bool charged) {
    if (over) { if (T - deadT > 1.f) resetGame(); return; }
    if (pst == 2 || pst == 3) return;
    if (charged) { cb = 3; chg = true; }
    else { chg = false; cb = (T - plastAtk < .7f) ? (cb % 3) + 1 : 1; }
    pst = 2; pt = 0; phit = false; plastAtk = T;
    sfx("audio/sfx_char_gumball_chop.ogg", .6f);
}

void GameLayer::spawnEnemy() {
    Enemy e;
    e.n = Node::create(); e.n->setCascadeOpacityEnabled(true);
    e.legs = Sprite::create(); e.armL = Sprite::create(); e.body = Sprite::create();
    e.head = Sprite::create(); e.armR = Sprite::create();
    e.n->addChild(e.legs); e.n->addChild(e.armL); e.n->addChild(e.body);
    e.n->addChild(e.head); e.n->addChild(e.armR);
    addChild(e.n);
    e.x = (rand() % 2) ? xmax + 25 : xmin - 25;
    e.y = 25 + rand() % 135;
    e.hp = 3; e.st = 0; e.t = 0; e.hurt = 0; e.f = 1; e.did = false;
    e.sp = 35 + rand() % 15 + wave * 3;
    E.push_back(e);
}

void GameLayer::update(float dt) {
    if (dt > .05f) dt = .05f;
    T += dt;
    if (!over) {
        Vec2 m = joyV;
        pt += dt;
        if (pinv > 0) pinv -= dt;
        if (pst == 2) {
            const Frames& a = ch[cb - 1];
            float len = a.size() / 14.f;
            if (!phit && pt > len * .5f) {
                phit = true;
                float r = chg ? 120.f : ((cb == 3) ? 95.f : 75.f);
                int dmg = chg ? 3 : ((cb == 3) ? 2 : 1);
                float kb = chg ? 70.f : ((cb == 3) ? 40.f : 14.f);
                for (auto& e : E) {
                    if (e.st != 2 && fabsf(e.y - py) < 35 && (e.x - px) * pf > -10 && fabsf(e.x - px) < r) {
                        e.hp -= dmg; e.hurt = .25f; e.x += pf * kb; score += 5;
                        sfx((chg || cb == 3) ? "audio/sfx_char_criticalhit.ogg" : "audio/sfx_char_impactnormal.ogg", .7f);
                        if (e.hp <= 0) { e.st = 2; e.t = 0; score += 10; sfx("audio/sfx_npc_breadman_death.ogg", .7f); }
                    }
                }
            }
            if (pt >= len) { pst = 0; pt = 0; chg = false; }
        } else if (pst == 3) {
            if (pt > .3f) { pst = 0; pt = 0; }
        } else {
            if (m.length() > .15f) {
                px += m.x * 110 * dt; py += m.y * 80 * dt;
                if (fabsf(m.x) > .2f) pf = (m.x > 0) ? 1 : -1;
                pst = 1;
            } else pst = 0;
        }
        px = std::max(xmin, std::min(xmax, px));
        py = std::max(20.f, std::min(160.f, py));

        spawnT -= dt;
        int alive = 0;
        for (auto& e : E) if (e.st != 2) alive++;
        if (spawnT <= 0 && alive < 2 + wave) { spawnT = std::max(1.2f, 3.2f - wave * .2f); spawnEnemy(); }
        if (score >= wave * 80) wave++;

        for (auto& e : E) {
            e.t += dt;
            if (e.hurt > 0) e.hurt -= dt;
            if (e.st == 2) continue;
            float dx = px - e.x, dy = py - e.y;
            e.f = (dx > 0) ? 1 : -1;
            if (e.st == 0) {
                if (e.hurt <= 0) {
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (fabsf(dx) < 62 && fabsf(dy) < 22) { e.st = 1; e.t = 0; e.did = false; sfx("audio/sfx_npc_breadman_attack.ogg", .5f); }
                    else if (dist > .1f) { e.x += dx / dist * e.sp * dt; e.y += dy / dist * e.sp * .7f * dt; }
                }
            } else if (e.st == 1) {
                if (!e.did && e.t > .36f) {
                    e.did = true;
                    if (fabsf(px - e.x) < 75 && fabsf(py - e.y) < 30 && pinv <= 0) {
                        hp -= 12; pinv = .6f; pst = 3; pt = 0; chg = false;
                        if (hp <= 0) {
                            over = true; deadT = T; pst = 4; pt = 0; if (score > best) best = score;
                            sfx("audio/sfx_char_gumball_faint.ogg", .8f);
                        } else {
                            sfx(StringUtils::format("audio/sfx_char_gumball_hurt%d.ogg", 1 + rand() % 4), .7f);
                        }
                    }
                }
                if (e.t > .65f) { e.st = 0; e.t = 0; }
            }
        }
        for (size_t i = 0; i < E.size();) {
            if (E[i].st == 2 && E[i].t > 1.2f) { E[i].n->removeFromParent(); E.erase(E.begin() + i); }
            else i++;
        }
    } else pt += dt;
    render();
}

void GameLayer::render() {
    const float S = .8f, SE = .5f;
    pn->setPosition(px, py + 70 * S);
    pn->setScaleX(pf * S); pn->setScaleY(S);
    pn->setLocalZOrder(1000 - (int)py);
    pn->setOpacity((pinv > 0 && ((int)(T * 20) % 2)) ? 120 : 255);
    float held = (atkId >= 0 && !over) ? (T - atkStart) : 0.f;
    if (held >= .8f) pn->setColor(((int)(T * 12) % 2) ? Color3B(255, 255, 90) : Color3B(255, 190, 40));
    else if (held >= .3f) pn->setColor(Color3B(255, 235, 140));
    else pn->setColor(Color3B::WHITE);
    if (pst == 2) { setF(pBody, pick(ch[cb - 1], pt, false)); pL->setVisible(false); pR->setVisible(false); }
    else if (pst == 1) { setF(pL, pick(runL, pt, true)); setF(pBody, pick(run, pt, true)); setF(pR, pick(runR, pt, true)); }
    else if (pst == 4) { setF(pBody, pick(death, pt, false)); pL->setVisible(false); pR->setVisible(false); }
    else if (pst == 3) { setF(pBody, frm("char_gumball_hit1.png")); pL->setVisible(false); pR->setVisible(false); }
    else { pL->setVisible(false); setF(pBody, pick(idle, pt, true)); setF(pR, pick(idleA, pt, true)); }

    for (auto& e : E) {
        e.n->setPosition(e.x, e.y + 62 * SE);
        e.n->setScaleX(e.f * SE); e.n->setScaleY(SE);
        e.n->setLocalZOrder(1000 - (int)e.y);
        if (e.st == 2) {
            e.n->setOpacity((GLubyte)(255 * std::max(0.f, 1.f - e.t / 1.2f)));
            e.legs->setVisible(false); e.armL->setVisible(false); e.head->setVisible(false); e.armR->setVisible(false);
            setF(e.body, frm("npc_breadman_death.png"));
        } else if (e.st == 1) {
            e.n->setOpacity(255);
            setF(e.legs, frm(e.t < .3f ? "npc_breadman_attack_legs1.png" : "npc_breadman_attack_legs2.png"));
            e.armL->setVisible(false); e.body->setVisible(false); e.armR->setVisible(false);
            setF(e.head, pick(atkH, e.t, false));
        } else {
            e.n->setOpacity(255);
            setF(e.legs, pick(legs, e.t, true)); setF(e.armL, frm("npc_breadman_run_l_arm.png"));
            setF(e.body, frm("npc_breadman_body1.png")); setF(e.head, frm("npc_breadman_run_head.png"));
            setF(e.armR, frm("npc_breadman_run_r_arm.png"));
        }
    }

    hud->clear();
    float bx = org.x + 10, by = org.y + vs.height - 22;
    hud->drawSolidRect(Vec2(bx, by), Vec2(bx + 124, by + 12), Color4F(0, 0, 0, .55f));
    hud->drawSolidRect(Vec2(bx + 2, by + 2), Vec2(bx + 2 + 120 * std::max(0, hp) / 100.f, by + 10),
                       hp > 30 ? Color4F(.37f, .81f, .37f, 1) : Color4F(.9f, .28f, .24f, 1));
    lbl->setString(StringUtils::format("Pontos %d  Onda %d", score, wave));
    if (over) lblOver->setString(StringUtils::format("Fim de jogo\nPontos %d  Recorde %d\nToque para jogar de novo", score, best));
    else lblOver->setString("");
}

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
    auto scene = Scene::create();
    scene->addChild(GameLayer::create());
    director->runWithScene(scene);
    return true;
}

void AppDelegate::applicationDidEnterBackground() { Director::getInstance()->stopAnimation(); AudioEngine::pauseAll(); }
void AppDelegate::applicationWillEnterForeground() { Director::getInstance()->startAnimation(); AudioEngine::resumeAll(); }
