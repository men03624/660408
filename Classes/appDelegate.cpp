#include "AppDelegate.h"
#include "cocos2d.h"
#include "audio/include/AudioEngine.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
USING_NS_CC;
namespace cocos2d { namespace experimental {} }
using namespace cocos2d::experimental;

typedef std::vector<SpriteFrame*> Frames;

static void sfx(const std::string& f, float v = .7f) { AudioEngine::play2d(f, false, v); }
static SpriteFrame* frm(const std::string& n) { return SpriteFrameCache::getInstance()->getSpriteFrameByName(n); }
static SpriteFrame* frmAny(const std::string& p, int i) {
    SpriteFrame* f = frm(StringUtils::format("%s_%03d.png", p.c_str(), i));
    if (!f) f = frm(StringUtils::format("%s%d.png", p.c_str(), i));
    if (!f) f = frm(StringUtils::format("%s_%d.png", p.c_str(), i));
    return f;
}
static Frames seqp(const std::string& p, int a, int b) {
    Frames v;
    for (int i = a; i <= b; i++) { SpriteFrame* f = frmAny(p, i); if (f) v.push_back(f); }
    return v;
}
static SpriteFrame* pick(const Frames& v, float t, bool loop) {
    if (v.empty()) return nullptr;
    int n = (int)v.size(), i = (int)(t * 14);
    i = loop ? i % n : std::min(n - 1, i);
    return v[i];
}
static SpriteFrame* pickN(const Frames& v, float prog) {
    if (v.empty()) return nullptr;
    int n = (int)v.size(), i = (int)(prog * n);
    return v[std::max(0, std::min(n - 1, i))];
}
static void setF(Sprite* s, SpriteFrame* f) {
    if (f) { s->setSpriteFrame(f); s->setVisible(true); } else s->setVisible(false);
}
static float vf(const ValueMap& m, const char* k, float def = 0) {
    auto it = m.find(k); return it == m.end() ? def : it->second.asFloat();
}
static std::string vstr(const ValueMap& m, const char* k) {
    auto it = m.find(k); return it == m.end() ? std::string("") : it->second.asString();
}
static Vec2 vpt(const std::string& s) {
    float a = 0, b = 0; sscanf(s.c_str(), "{%f,%f}", &a, &b); return Vec2(a, b);
}

/* ---------- inimigos ---------- */
enum { T_BREAD, T_BROC, T_CARROT, T_STEAK, T_CHEESE, T_N };
struct TypeDef { const char* key; float hp, sp, dmg, range, hitT, dur, scale, foot; };
static const TypeDef TD[T_N] = {
    {"BreadMan", 5, 35, 2, 62, .36f, .65f, .5f, 62},
    {"Broccoli", 3, 38, 1, 58, .45f, .85f, 1.f, 60},
    {"Carrot",   2, 55, 1, 55, .40f, .80f, 1.f, 60},
    {"Steak",    1, 75, 1, 40, .35f, .60f, 1.f, 25},
    {"Cheese",   3, 40, 2,  0,   0,    0, 1.f, 22}};

static int typeOf(const std::string& n) {
    for (int i = 0; i < T_N; i++) if (n == TD[i].key) return i;
    return T_BROC;
}
static void attackSfx(int t) {
    switch (t) {
        case T_BREAD: sfx("audio/sfx_npc_breadman_attack.ogg", .5f); break;
        case T_STEAK: sfx("audio/sfx_npc_steak_attack.ogg", .5f); break;
        case T_CHEESE: sfx("audio/sfx_npc_cheesewheel_attack.ogg", .5f); break;
        default: sfx("audio/sfx_npc_broccoli_attack1.ogg", .5f); break;
    }
}
static void deathSfx(int t) {
    switch (t) {
        case T_BREAD: sfx("audio/sfx_npc_breadman_death.ogg", .7f); break;
        case T_STEAK: sfx("audio/sfx_npc_steak_death.ogg", .7f); break;
        case T_CHEESE: sfx("audio/sfx_npc_cheesewheel_death.ogg", .7f); break;
        default: sfx(StringUtils::format("audio/sfx_npc_broccoli_death%d.ogg", 1 + rand() % 3), .7f); break;
    }
}

struct EF {
    Frames a, b, c, d, e, f;
    SpriteFrame* s[6];
    EF() { for (int i = 0; i < 6; i++) s[i] = nullptr; }
};
static EF EFR[T_N];

static void loadEnemyFrames() {
    EF& b = EFR[T_BREAD];
    b.a = seqp("npc_breadman_run_legs", 0, 12);
    b.b = seqp("npc_breadman_attack_headarms", 0, 8);
    b.s[0] = frm("npc_breadman_body1.png");
    b.s[1] = frm("npc_breadman_run_head.png");
    b.s[2] = frm("npc_breadman_run_l_arm.png");
    b.s[3] = frm("npc_breadman_run_r_arm.png");
    b.s[4] = frm("npc_breadman_death.png");
    for (int t = T_BROC; t <= T_CARROT; t++) {
        std::string p = (t == T_BROC) ? "npc_broccoli" : "npc_carrot";
        EF& e = EFR[t];
        e.a = seqp(p + "_run_legs", 0, 14);
        e.b = seqp(p + "_run_l_arm", 0, 14);
        e.c = seqp(p + "_run_r_arm", 0, 14);
        e.d = seqp(p + "_attack_l_arm", 0, 20);
        e.e = seqp(p + "_attack_r_arm", 0, 20);
        e.f = seqp(p + "_death", 0, 4);
        e.s[0] = frm(p + "_hit_body1.png");
        e.s[1] = frm(p + "_eyes1.png");
        e.s[2] = frm(p + "_eyes2.png");
        e.s[3] = frm(p + "_mouth1.png");
        e.s[4] = frm(p + "_mouth2.png");
    }
    EF& st = EFR[T_STEAK];
    st.a = seqp("npc_steak_idle", 0, 10);
    st.b = seqp("npc_steak_attack_bite", 0, 8);
    st.c = seqp("npc_steak_attack_bite_arms", 0, 8);
    st.s[0] = frm("npc_steak_hit.png");
    if (!st.s[0]) st.s[0] = frm("npc_steak_hit1.png");
    EF& ch = EFR[T_CHEESE];
    ch.b = seqp("npc_cheese_charge", 0, 12);
    ch.c = seqp("npc_cheese_dizzy", 0, 8);
    ch.f = seqp("npc_cheese_death", 0, 6);
    ch.s[0] = frm("npc_cheese_static.png");
}

/* ---------- personagens ---------- */
enum { H_GUM, H_DAR, H_ANA, H_N };
static const char* HN[3] = {"gumball", "darwin", "anais"};
struct HF {
    Frames idle, idleA, run, runA, runB, atk[3], chg, hold, death, hit, shootH, shot;
    SpriteFrame* s[4];
    HF() { for (int i = 0; i < 4; i++) s[i] = nullptr; }
};
static HF HFR[H_N];

static void loadHeroFrames() {
    HF& g = HFR[H_GUM];
    g.idle = seqp("char_gumball_idle", 0, 7);
    g.idleA = seqp("char_gumball_idle_arms", 0, 7);
    g.run = seqp("char_gumball_run", 0, 9);
    g.runA = seqp("char_gumball_run_larm", 0, 9);
    g.runB = seqp("char_gumball_run_rarm", 0, 9);
    g.atk[0] = seqp("char_gumball_chop1", 0, 3);
    g.atk[1] = seqp("char_gumball_chop2", 0, 3);
    g.atk[2] = seqp("char_gumball_chop3", 0, 7);
    g.chg = g.atk[2];
    g.death = seqp("char_gumball_death", 0, 6);
    g.hit = seqp("char_gumball_hit", 1, 2);

    HF& a = HFR[H_ANA];
    a.idle = seqp("char_anais_idle", 0, 7);
    a.idleA = seqp("char_anais_idle_arms", 0, 7);
    a.run = seqp("char_anais_run", 0, 7);
    a.runA = seqp("char_anais_run_arms", 0, 7);
    a.atk[0] = seqp("char_anais_punch1", 0, 3);
    a.atk[1] = seqp("char_anais_punch2", 0, 3);
    a.atk[2] = seqp("char_anais_punch3", 0, 6);
    a.chg = seqp("char_anais_punch4", 0, 7);
    a.hold = seqp("char_anais_charge_hold", 0, 11);
    a.death = seqp("char_anais_death", 0, 5);
    a.hit = seqp("char_anais_hit", 1, 2);

    HF& d = HFR[H_DAR];
    d.idle = seqp("char_darwin_idle_body", 0, 11);
    d.idleA = seqp("char_darwin_idle_arms", 0, 11);
    d.run = seqp("char_darwin_run_legs", 0, 8);
    d.runA = seqp("char_darwin_run_arms", 0, 8);
    d.runB = seqp("char_darwin_run_arms_r", 0, 8);
    d.s[0] = frm("char_darwin_idle_legs.png");
    d.s[1] = frmAny("char_darwin_run_body", 0);
    d.s[2] = frm("char_darwin_shoot_standing_legs.png");
    d.shootH = seqp("char_darwin_shoot_head", 0, 3);
    d.chg = seqp("char_darwin_charge_release", 0, 1);
    d.hold = seqp("char_darwin_charge_hold", 0, 11);
    d.death = seqp("char_darwin_death", 0, 9);
    d.hit = seqp("char_darwin_hit", 1, 2);
    d.shot = seqp("char_darwin_watershot", 0, 1);
}

/* ---------- fases ---------- */
struct Group { int type; int count; };
struct Range { float t0, t1, rate, cap; };
struct Level {
    std::string key, bg, startChar;
    int ch, no;
    float dur, minY, maxY, down;
    Vec2 start;
    std::vector<Group> groups;
    std::vector<Range> ranges;
    bool ok;
    Level() : ch(0), no(0), dur(0), minY(0), maxY(260), down(.25f), ok(false) {}
};

struct Enemy {
    int type;
    Node* n; Sprite* sp[6];
    float x, y, t, hp, hurt, spd, dir;
    int st, f, pid; bool did;   // st: 0 anda, 1 ataca, 2 morto, 3 rola, 4 tonto, 5 prepara
};

struct Proj {
    Sprite* s;
    float x, y, vx, life, t, sc;
    int dmg, id; bool pierce;
};

class GameLayer : public Layer {
public:
    CREATE_FUNC(GameLayer);
    bool init() override;
    void update(float dt) override;
private:
    void loadLevels();
    int nextPlayable(int from);
    void startLevel(int idx);
    void attack(bool charged);
    void spawnEnemy(int type);
    void hitPlayer(int dmg);
    void killEnemy(Enemy& e);
    void switchTo(int i);
    void cheese(Enemy& e, float dx, float dy, float dt);
    float persp(float y) const;
    float atkLen() const;
    Vec2 toScr(float x, float y) const { return Vec2(bgL + x * ws, org.y + y * ws); }
    Vec2 portPos(int i) const { return Vec2(org.x + 42 + i * 60, org.y + vs.height - 30); }
    void renderHero(float held);
    void render();

    Size vs; Vec2 org; float cx = 0;
    std::vector<Level> LV;
    int cur = 0, unlocked = 0, phase = 0;   // phase: 0 jogando, 1 venceu, 2 perdeu
    float levelT = 0, phaseT = 0, spawnT = 0;
    Sprite* bgNode = nullptr;
    float ws = 1, bgL = 0, bgW = 651, bgH = 421;

    Node* pn = nullptr; Sprite* ps[5];
    float px = 200, py = 70, pt = 0, pinv = 0, plastAtk = -9, T = 0;
    int pf = 1, pst = 0, cb = 0, score = 0, act = 0, projId = 0;
    int hhp[3];
    bool phit = false, chg = false;
    std::vector<Enemy> E;
    std::vector<Proj> P;
    Vec2 joyO, joyV; int joyId = -1;
    int atkId = -1; float atkStart = 0;
    Sprite *port[3], *fnt[3], *heartsSpr = nullptr;
    Label *lbl = nullptr, *lblTitle = nullptr, *lblOver = nullptr;
};

void GameLayer::loadLevels() {
    ValueMap all = FileUtils::getInstance()->getValueMapFromFile("data/GameLevelSetup.plist");
    for (auto& kv : all) {
        int c = 0, n = 0;
        if (sscanf(kv.first.c_str(), "chapter-%d-level-%d", &c, &n) != 2) continue;
        const ValueMap& m = kv.second.asValueMap();
        Level L;
        L.key = kv.first; L.ch = c; L.no = n;
        L.dur = vf(m, "Duration", 0);
        L.start = vpt(vstr(m, "StartPosition"));
        L.minY = vf(m, "MinYScalePosition", 90);
        L.maxY = vf(m, "MaxYScalePosition", 260);
        L.down = vf(m, "MaxAmountScaledDown", .25f);
        L.bg = vstr(m, "PhysicsEditorName");
        L.startChar = vstr(m, "StartCharacter");
        auto sp = m.find("SpawnPoints");
        if (sp != m.end()) {
            for (auto& g : sp->second.asValueMap()) {
                const ValueMap& gm = g.second.asValueMap();
                auto cf = gm.find("CCBFile");
                if (cf == gm.end()) continue;
                std::string f = cf->second.asString();
                size_t b = f.find("Spawns");
                if (b == std::string::npos || b < 4) continue;
                Group gr;
                gr.type = typeOf(f.substr(4, b - 4));
                gr.count = std::max(1, (int)vf(gm, "EnemiesSpawned", 1));
                L.groups.push_back(gr);
            }
        }
        auto tr = m.find("SpawnTimeRanges");
        if (tr != m.end()) {
            for (auto& r : tr->second.asValueVector()) {
                const ValueMap& rm = r.asValueMap();
                Vec2 tt = vpt(vstr(rm, "SpawnTimeRange"));
                Range R; R.t0 = tt.x; R.t1 = tt.y;
                R.rate = vf(rm, "SpawnRate", 1); R.cap = vf(rm, "SpawnCap", 3);
                L.ranges.push_back(R);
            }
        }
        L.ok = (L.dur > 0 && !L.groups.empty() && !L.ranges.empty());
        LV.push_back(L);
    }
    std::sort(LV.begin(), LV.end(), [](const Level& a, const Level& b) {
        return a.ch != b.ch ? a.ch < b.ch : a.no < b.no;
    });
}

int GameLayer::nextPlayable(int from) {
    for (int i = std::max(0, from); i < (int)LV.size(); i++) if (LV[i].ok) return i;
    return -1;
}

bool GameLayer::init() {
    if (!Layer::init()) return false;
    auto d = Director::getInstance();
    vs = d->getVisibleSize(); org = d->getVisibleOrigin(); cx = org.x + vs.width / 2;
    auto cache = SpriteFrameCache::getInstance();
    const char* bgs[4] = {"stage1", "stage1b", "stage2", "stage3"};
    for (int i = 0; i < 4; i++) {
        cache->addSpriteFramesWithFile(StringUtils::format("data/mfm-game-%s-bg.plist", bgs[i]),
                                       StringUtils::format("textures/mfm-game-%s-bg.png", bgs[i]));
    }
    cache->addSpriteFramesWithFile("data/mfm-game-gumball-sprites.plist", "textures/mfm-game-gumball-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-anais-sprites.plist", "textures/mfm-game-anais-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-sprites.plist", "textures/mfm-game-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-npcbreadman-sprites.plist", "textures/mfm-game-npcbreadman-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-npc1-sprites.plist", "textures/mfm-game-npc1-sprites.png");

    loadHeroFrames();
    loadEnemyFrames();
    loadLevels();

    pn = Node::create(); pn->setCascadeOpacityEnabled(true); pn->setCascadeColorEnabled(true);
    for (int i = 0; i < 5; i++) { ps[i] = Sprite::create(); pn->addChild(ps[i]); }
    addChild(pn);

    for (int i = 0; i < 3; i++) {
        port[i] = Sprite::create(); fnt[i] = Sprite::create();
        addChild(port[i], 5003); addChild(fnt[i], 5004);
        hhp[i] = 6;
    }
    heartsSpr = Sprite::create(); addChild(heartsSpr, 5003);

    lbl = Label::createWithSystemFont("", "Arial", 16);
    lbl->setAnchorPoint(Vec2(1, 1)); lbl->setPosition(org.x + vs.width - 10, org.y + vs.height - 8);
    addChild(lbl, 5001);
    lblTitle = Label::createWithSystemFont("", "Arial", 16);
    lblTitle->setAnchorPoint(Vec2(.5f, 1)); lblTitle->setPosition(cx + 40, org.y + vs.height - 8);
    addChild(lblTitle, 5001);
    lblOver = Label::createWithSystemFont("", "Arial", 22);
    lblOver->setPosition(cx, org.y + vs.height / 2);
    addChild(lblOver, 5002);

    auto l = EventListenerTouchAllAtOnce::create();
    l->onTouchesBegan = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) {
            Vec2 p = t->getLocation();
            bool onPort = false;
            for (int i = 0; i < 3; i++) {
                if ((p - portPos(i)).length() < 30.f) { switchTo(i); onPort = true; }
            }
            if (onPort) continue;
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

    unlocked = UserDefault::getInstance()->getIntegerForKey("mfm_unlocked", 0);
    int first = nextPlayable(unlocked);
    if (first < 0) first = nextPlayable(0);
    if (first < 0) first = 0;
    startLevel(first);
    AudioEngine::play2d("audio/music_chapter1.mp3", true, 0.4f);
    scheduleUpdate();
    return true;
}

float GameLayer::persp(float y) const {
    const Level& L = LV[cur];
    float r = (L.maxY > L.minY) ? (y - L.minY) / (L.maxY - L.minY) : 0.f;
    r = std::max(0.f, std::min(1.f, r));
    return 1.f - L.down * r;
}

void GameLayer::startLevel(int idx) {
    if (idx < 0 || idx >= (int)LV.size()) idx = 0;
    cur = idx;
    const Level& L = LV[cur];
    for (auto& e : E) e.n->removeFromParent();
    E.clear();
    for (auto& p : P) p.s->removeFromParent();
    P.clear();
    if (bgNode) { bgNode->removeFromParent(); bgNode = nullptr; }
    SpriteFrame* bf = frm(L.bg + ".png");
    if (!bf) bf = frm("stages_c1_mainbg.png");
    if (bf) {
        bgNode = Sprite::createWithSpriteFrame(bf);
        Size bs = bgNode->getContentSize();
        bgW = bs.width; bgH = bs.height;
        ws = std::max(vs.width / bgW, vs.height / bgH);
        bgNode->setScale(ws);
        bgNode->setAnchorPoint(Vec2(.5f, 0.f));
        bgNode->setPosition(cx, org.y);
        addChild(bgNode, -5000);
        bgL = cx - bgW * ws / 2;
    }
    px = (L.start.x > 0) ? L.start.x : bgW / 2;
    py = (L.start.y > 0) ? L.start.y : 100;
    px = std::max(25.f, std::min(bgW - 25.f, px));
    py = std::max(15.f, std::min(L.maxY - 15.f, py));
    for (int i = 0; i < 3; i++) hhp[i] = 6;
    act = (L.startChar == "Darwin") ? H_DAR : (L.startChar == "Anais") ? H_ANA : H_GUM;
    pf = 1; pst = 0; pt = 0; score = 0; pinv = 0; plastAtk = -9; cb = 0;
    chg = false; levelT = 0; phase = 0; spawnT = .6f;
    joyId = -1; joyV = Vec2::ZERO; atkId = -1;
}

void GameLayer::switchTo(int i) {
    if (phase != 0 || i == act || hhp[i] <= 0) return;
    act = i; pst = 0; pt = 0; chg = false; cb = 0; atkId = -1;
    sfx("audio/sfx_gui_wheel_tap.ogg", .6f);
}

float GameLayer::atkLen() const {
    if (act == H_DAR) return chg ? .4f : .32f;
    const HF& F = HFR[act];
    const Frames& a = chg ? F.chg : F.atk[std::max(1, cb) - 1];
    return std::max(.2f, a.size() / 14.f);
}

void GameLayer::attack(bool charged) {
    if (phase != 0) {
        if (T - phaseT > 1.f) {
            if (phase == 1) {
                int nx = nextPlayable(cur + 1);
                startLevel(nx >= 0 ? nx : nextPlayable(0));
            } else startLevel(cur);
        }
        return;
    }
    if (pst == 2 || pst == 3) return;
    if (charged) { cb = 3; chg = true; }
    else { chg = false; cb = (T - plastAtk < .7f) ? (cb % 3) + 1 : 1; }
    pst = 2; pt = 0; phit = false; plastAtk = T;
    if (act == H_GUM) sfx("audio/sfx_char_gumball_chop.ogg", .6f);
    else if (act == H_ANA) sfx(StringUtils::format("audio/sfx_char_anais_punch%d.ogg", chg ? 5 : cb), .6f);
}

void GameLayer::spawnEnemy(int type) {
    const Level& L = LV[cur];
    Enemy e;
    e.type = type;
    e.n = Node::create(); e.n->setCascadeOpacityEnabled(true);
    for (int i = 0; i < 6; i++) { e.sp[i] = Sprite::create(); e.n->addChild(e.sp[i]); }
    addChild(e.n);
    e.x = (rand() % 2) ? bgW + 30 : -30;
    e.y = 20 + rand() % (int)std::max(30.f, L.maxY - 40);
    e.hp = TD[type].hp; e.t = 0; e.hurt = 0; e.dir = 1;
    e.spd = TD[type].sp * (.9f + (rand() % 20) / 100.f);
    e.st = 0; e.f = 1; e.did = false; e.pid = -1;
    E.push_back(e);
}

void GameLayer::hitPlayer(int dmg) {
    if (pinv > 0 || phase != 0) return;
    hhp[act] = std::max(0, hhp[act] - dmg);
    pinv = .8f; chg = false;
    if (hhp[act] <= 0) {
        sfx(act == H_GUM ? "audio/sfx_char_gumball_faint.ogg"
            : (act == H_DAR ? "audio/sfx_char_darwin_faint.ogg" : "audio/sfx_char_anais_faint.ogg"), .8f);
        int nx = -1;
        for (int k = 1; k <= 2; k++) { int j = (act + k) % 3; if (hhp[j] > 0) { nx = j; break; } }
        if (nx < 0) { phase = 2; phaseT = T; pst = 4; pt = 0; }
        else { act = nx; pst = 0; pt = 0; pinv = 1.2f; cb = 0; atkId = -1; sfx("audio/sfx_gui_wheel_tap.ogg", .6f); }
    } else {
        pst = 3; pt = 0;
        if (act == H_GUM) sfx(StringUtils::format("audio/sfx_char_gumball_hurt%d.ogg", 1 + rand() % 4), .7f);
        else if (act == H_DAR) sfx(StringUtils::format("audio/sfx_char_darwin_hurt%d.ogg", 1 + rand() % 2), .7f);
        else sfx(StringUtils::format("audio/sfx_char_anais_hurt%d.ogg", 1 + rand() % 2), .7f);
    }
}

void GameLayer::killEnemy(Enemy& e) {
    e.st = 2; e.t = 0; score += 10;
    deathSfx(e.type);
}

void GameLayer::cheese(Enemy& e, float dx, float dy, float dt) {
    if (e.st == 0) {
        e.f = (dx > 0) ? 1 : -1;
        if (e.hurt <= 0) {
            if (fabsf(dy) < 22 && fabsf(dx) > 70 && fabsf(dx) < 300) { e.st = 5; e.t = 0; attackSfx(T_CHEESE); }
            else {
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > .1f) { e.x += dx / dist * e.spd * dt; e.y += dy / dist * e.spd * .7f * dt; }
            }
        }
    } else if (e.st == 5) {
        if (e.t > .4f) { e.st = 3; e.t = 0; e.dir = (dx > 0) ? 1.f : -1.f; e.did = false; }
    } else if (e.st == 3) {
        e.x += e.dir * 230 * dt; e.f = (e.dir > 0) ? 1 : -1;
        if (!e.did && fabsf(px - e.x) < 45 && fabsf(py - e.y) < 28) { e.did = true; hitPlayer((int)TD[T_CHEESE].dmg); }
        if (e.t > .9f || e.x < 0 || e.x > bgW) { e.st = 4; e.t = 0; }
    } else if (e.st == 4) {
        if (e.t > 1.2f) { e.st = 0; e.t = 0; }
    }
}

void GameLayer::update(float dt) {
    if (dt > .05f) dt = .05f;
    T += dt;
    const Level& L = LV[cur];
    if (phase == 0) {
        levelT += dt;
        Vec2 m = joyV;
        pt += dt;
        if (pinv > 0) pinv -= dt;
        if (pst == 2) {
            float len = atkLen();
            float hitT = (act == H_DAR) ? (chg ? .12f : .14f) : len * .5f;
            if (!phit && pt > hitT) {
                phit = true;
                if (act == H_DAR) {
                    Proj pr;
                    pr.s = Sprite::create(); addChild(pr.s);
                    pr.x = px + pf * 28; pr.y = py + 42;
                    pr.vx = pf * (chg ? 300.f : 340.f);
                    pr.life = 1.1f; pr.t = 0; pr.sc = chg ? 1.8f : 1.f;
                    pr.dmg = chg ? 3 : 1; pr.pierce = chg; pr.id = ++projId;
                    P.push_back(pr);
                    sfx(chg ? "audio/sfx_char_darwin_chargerelease.ogg" : "audio/sfx_char_darwin_shoot.ogg", .6f);
                } else {
                    float base = (act == H_ANA) ? 85.f : 75.f;
                    float r = chg ? base + 45.f : ((cb == 3) ? base + 20.f : base);
                    int dmg = chg ? 3 : ((cb == 3) ? 2 : 1);
                    float kb = chg ? 70.f : ((cb == 3) ? 40.f : 14.f);
                    for (auto& e : E) {
                        if (e.st != 2 && fabsf(e.y - py) < 35 && (e.x - px) * pf > -10 && fabsf(e.x - px) < r) {
                            e.hp -= dmg; e.hurt = .25f; e.x += pf * kb; score += 5;
                            sfx((chg || cb == 3) ? "audio/sfx_char_criticalhit.ogg" : "audio/sfx_char_impactnormal.ogg", .7f);
                            if (e.hp <= 0) killEnemy(e);
                        }
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
        px = std::max(25.f, std::min(bgW - 25.f, px));
        py = std::max(15.f, std::min(L.maxY - 15.f, py));

        for (size_t i = 0; i < P.size();) {
            Proj& p = P[i];
            p.t += dt; p.x += p.vx * dt; p.life -= dt;
            bool rm = (p.life <= 0 || p.x < -40 || p.x > bgW + 40);
            if (!rm) {
                for (auto& e : E) {
                    if (e.st == 2 || e.pid == p.id) continue;
                    if (fabsf(e.x - p.x) < 30 && fabsf(e.y - (p.y - 42)) < 30) {
                        e.pid = p.id; e.hp -= p.dmg; e.hurt = .25f;
                        e.x += (p.vx > 0) ? 8.f : -8.f; score += 5;
                        sfx("audio/sfx_char_darwin_shootimpact.ogg", .6f);
                        if (e.hp <= 0) killEnemy(e);
                        if (!p.pierce) { rm = true; break; }
                    }
                }
            }
            if (rm) { p.s->removeFromParent(); P.erase(P.begin() + i); }
            else i++;
        }

        int alive = 0;
        for (auto& e : E) if (e.st != 2) alive++;

        spawnT -= dt;
        if (levelT < L.dur && spawnT <= 0) {
            const Range* r = nullptr;
            for (auto& g : L.ranges) if (levelT >= g.t0 && levelT <= g.t1 + 1.f) r = &g;
            if (!r && !L.ranges.empty()) r = &L.ranges.back();
            if (r && !L.groups.empty() && alive < (int)r->cap) {
                const Group& g = L.groups[rand() % L.groups.size()];
                for (int k = 0; k < g.count; k++) spawnEnemy(g.type);
            }
            spawnT = r ? std::max(.8f, std::min(4.f, 3.5f / std::max(1.f, r->rate))) : .5f;
        }

        for (auto& e : E) {
            e.t += dt;
            if (e.hurt > 0) e.hurt -= dt;
            if (e.st == 2) continue;
            const TypeDef& d = TD[e.type];
            float dx = px - e.x, dy = py - e.y;
            if (e.type == T_CHEESE) { cheese(e, dx, dy, dt); continue; }
            e.f = (dx > 0) ? 1 : -1;
            if (e.st == 0) {
                if (e.hurt <= 0) {
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (fabsf(dx) < d.range && fabsf(dy) < 22) { e.st = 1; e.t = 0; e.did = false; attackSfx(e.type); }
                    else if (dist > .1f) { e.x += dx / dist * e.spd * dt; e.y += dy / dist * e.spd * .7f * dt; }
                }
            } else if (e.st == 1) {
                if (!e.did && e.t > d.hitT) {
                    e.did = true;
                    if (fabsf(px - e.x) < d.range + 13 && fabsf(py - e.y) < 30) hitPlayer((int)d.dmg);
                }
                if (e.t > d.dur) { e.st = 0; e.t = 0; }
            }
        }
        for (size_t i = 0; i < E.size();) {
            if (E[i].st == 2 && E[i].t > 1.2f) { E[i].n->removeFromParent(); E.erase(E.begin() + i); }
            else i++;
        }

        alive = 0;
        for (auto& e : E) if (e.st != 2) alive++;
        if (phase == 0 && levelT >= L.dur && alive == 0) {
            phase = 1; phaseT = T;
            int nx = nextPlayable(cur + 1);
            if (nx > unlocked) {
                unlocked = nx;
                UserDefault::getInstance()->setIntegerForKey("mfm_unlocked", unlocked);
                UserDefault::getInstance()->flush();
            }
            sfx("audio/sfx_gui_achievementcomplete.ogg", .7f);
        }
    } else pt += dt;
    render();
}

void GameLayer::renderHero(float held) {
    const HF& F = HFR[act];
    for (int i = 0; i < 5; i++) ps[i]->setVisible(false);
    if (pst == 4) { setF(ps[1], pick(F.death, pt, false)); return; }
    if (pst == 3) { setF(ps[1], pick(F.hit, pt, true)); return; }
    if (pst == 2) {
        if (act == H_DAR) {
            if (chg) setF(ps[1], pickN(F.chg, pt / .4f));
            else { setF(ps[0], F.s[2]); setF(ps[1], pickN(F.shootH, pt / .32f)); }
        } else {
            const Frames& a = chg ? F.chg : F.atk[std::max(1, cb) - 1];
            setF(ps[1], pick(a, pt, false));
        }
        return;
    }
    if (held >= .3f && !F.hold.empty()) { setF(ps[1], pick(F.hold, held, true)); return; }
    if (pst == 1) {
        if (act == H_DAR) {
            setF(ps[0], pick(F.run, pt, true)); setF(ps[1], F.s[1]);
            setF(ps[2], pick(F.runA, pt, true)); setF(ps[3], pick(F.runB, pt, true));
        } else if (act == H_GUM) {
            setF(ps[0], pick(F.runA, pt, true)); setF(ps[1], pick(F.run, pt, true)); setF(ps[2], pick(F.runB, pt, true));
        } else {
            setF(ps[1], pick(F.run, pt, true)); setF(ps[2], pick(F.runA, pt, true));
        }
        return;
    }
    if (act == H_DAR) setF(ps[0], F.s[0]);
    setF(ps[1], pick(F.idle, pt, true));
    setF(ps[2], pick(F.idleA, pt, true));
}

void GameLayer::render() {
    const Level& L = LV[cur];
    float ps_ = ws * persp(py);
    Vec2 sp = toScr(px, py);
    pn->setPosition(sp.x, sp.y + 70 * ps_);
    pn->setScaleX(pf * ps_); pn->setScaleY(ps_);
    pn->setLocalZOrder(1000 - (int)sp.y);
    pn->setOpacity((pinv > 0 && ((int)(T * 20) % 2)) ? 120 : 255);
    float held = (atkId >= 0 && phase == 0) ? (T - atkStart) : 0.f;
    if (held >= .8f) pn->setColor(((int)(T * 12) % 2) ? Color3B(255, 255, 90) : Color3B(255, 190, 40));
    else if (held >= .3f) pn->setColor(Color3B(255, 235, 140));
    else pn->setColor(Color3B::WHITE);
    renderHero(held);

    for (auto& p : P) {
        Vec2 q = toScr(p.x, p.y);
        float s = ws * persp(p.y - 42) * p.sc;
        p.s->setPosition(q);
        p.s->setScaleX((p.vx > 0 ? 1.f : -1.f) * s); p.s->setScaleY(s);
        p.s->setLocalZOrder(1000 - (int)q.y);
        setF(p.s, pick(HFR[H_DAR].shot, p.t, true));
    }

    for (auto& e : E) {
        const TypeDef& d = TD[e.type];
        const EF& f = EFR[e.type];
        float es = ws * persp(e.y) * d.scale;
        Vec2 q = toScr(e.x, e.y);
        e.n->setPosition(q.x, q.y + d.foot * es);
        e.n->setScaleX(e.f * es); e.n->setScaleY(es);
        e.n->setLocalZOrder(1000 - (int)q.y);
        e.n->setOpacity(255);
        for (int i = 0; i < 6; i++) e.sp[i]->setVisible(false);
        if (e.st == 2) {
            e.n->setOpacity((GLubyte)(255 * std::max(0.f, 1.f - e.t / 1.2f)));
            if (e.type == T_BREAD) setF(e.sp[2], f.s[4]);
            else if (e.type == T_BROC || e.type == T_CARROT) setF(e.sp[2], pickN(f.f, e.t / .5f));
            else if (e.type == T_STEAK) setF(e.sp[0], f.s[0]);
            else setF(e.sp[0], pickN(f.f, e.t / .5f));
            continue;
        }
        if (e.type == T_BREAD) {
            if (e.st == 1) {
                setF(e.sp[0], frm(e.t < .3f ? "npc_breadman_attack_legs1.png" : "npc_breadman_attack_legs2.png"));
                setF(e.sp[3], pick(f.b, e.t, false));
            } else {
                setF(e.sp[0], pick(f.a, e.t, true)); setF(e.sp[1], f.s[2]);
                setF(e.sp[2], f.s[0]); setF(e.sp[3], f.s[1]); setF(e.sp[5], f.s[3]);
            }
        } else if (e.type == T_BROC || e.type == T_CARROT) {
            if (e.st == 1) {
                float pr = e.t / d.dur;
                setF(e.sp[0], f.a.empty() ? nullptr : f.a[0]);
                setF(e.sp[1], pickN(f.d, pr)); setF(e.sp[2], f.s[0]);
                setF(e.sp[3], f.s[2] ? f.s[2] : f.s[1]); setF(e.sp[4], f.s[4]);
                setF(e.sp[5], pickN(f.e, pr));
            } else {
                setF(e.sp[0], pick(f.a, e.t, true)); setF(e.sp[1], pick(f.b, e.t, true));
                setF(e.sp[2], f.s[0]); setF(e.sp[3], f.s[1]); setF(e.sp[4], f.s[3]);
                setF(e.sp[5], pick(f.c, e.t, true));
            }
        } else if (e.type == T_STEAK) {
            if (e.st == 1) { setF(e.sp[0], pickN(f.b, e.t / d.dur)); setF(e.sp[1], pickN(f.c, e.t / d.dur)); }
            else setF(e.sp[0], pick(f.a, e.t, true));
        } else {
            if (e.st == 3) setF(e.sp[0], pick(f.b, e.t, true));
            else if (e.st == 4) setF(e.sp[0], pick(f.c, e.t, true));
            else setF(e.sp[0], f.s[0]);
        }
    }

    for (int i = 0; i < 3; i++) {
        int h = hhp[i];
        int lv = (h >= 5) ? 6 : (h >= 3) ? 4 : (h >= 1) ? 2 : 0;
        setF(port[i], frm(StringUtils::format("gui_character_%s_health%d.png", HN[i], lv)));
        port[i]->setPosition(portPos(i));
        port[i]->setScale(i == act ? .9f : .6f);
        port[i]->setOpacity(h <= 0 ? 90 : (i == act ? 255 : 170));
        if (h <= 0) { setF(fnt[i], frm("gui_character_fainted.png")); fnt[i]->setPosition(portPos(i)); fnt[i]->setScale(.6f); }
        else fnt[i]->setVisible(false);
    }
    Vec2 pa = portPos(act);
    setF(heartsSpr, frm(StringUtils::format("gui_character_hearts_%d.png", hhp[act])));
    heartsSpr->setPosition(pa.x, pa.y - 30); heartsSpr->setScale(.8f);

    int remain = std::max(0, (int)ceilf(L.dur - levelT));
    lblTitle->setString(StringUtils::format("Fase %d-%d    Tempo %d", L.ch, L.no, remain));
    lbl->setString(StringUtils::format("Pontos %d", score));
    if (phase == 1) lblOver->setString(StringUtils::format("Fase concluida!\nPontos %d\nToque para continuar", score));
    else if (phase == 2) lblOver->setString("Fim de jogo\nToque para tentar de novo");
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
