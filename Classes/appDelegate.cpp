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
static const char* ABIL[3] = {"gui_specials_gumball_sugarrush.png", "gui_specials_darwin_bubleshield.png", "gui_specials_anais_heal.png"};
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
    void landHit();
    void switchTo(int i);
    void useAbility();
    void cheese(Enemy& e, float dx, float dy, float dt);
    float persp(float y) const;
    float atkLen() const;
    Vec2 toScr(float x, float y) const { return Vec2(bgL + x * ws, org.y + y * ws); }
    Vec2 wheelC() const { return Vec2(org.x + vs.width - 70, org.y + vs.height - 62); }
    Vec2 abilPos() const { return Vec2(org.x + 40, org.y + vs.height - 44); }
    Vec2 pausePos() const { return Vec2(org.x + vs.width - 20, org.y + vs.height - 20); }
    Vec2 portPos(int i) const {
        int rel = (i - act + 3) % 3;
        if (rel == 0) return wheelC();
        if (rel == 1) return wheelC() + Vec2(54, -18);
        return wheelC() + Vec2(-48, 22);
    }
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
    float abilT[3];
    bool abilReady[3];
    float rushT = 0, shieldT = 0, healFx = 0, comboT = 0;
    int combo = 0;
    bool paused = false;
    bool phit = false, chg = false;
    std::vector<Enemy> E;
    std::vector<Proj> P;
    Vec2 joyO, joyV; int joyId = -1;
    int atkId = -1; float atkStart = 0;
    Sprite *port[3], *fnt[3], *heartsSpr = nullptr, *abilBtn = nullptr, *pauseBtn = nullptr;
    Sprite *comboTxt = nullptr, *comboRing = nullptr;
    DrawNode* hud = nullptr;
    Label *lbl = nullptr, *lblTitle = nullptr, *lblOver = nullptr, *lblCombo = nullptr;
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
    cache->addSpriteFramesWithFile("data/mfm-game-nice-sprites.plist", "textures/mfm-game-nice-sprites.png");
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
        hhp[i] = 6; abilT[i] = 8; abilReady[i] = false;
    }
    heartsSpr = Sprite::create(); addChild(heartsSpr, 5007);
    abilBtn = Sprite::create(); addChild(abilBtn, 5005);
    pauseBtn = Sprite::create(); addChild(pauseBtn, 5008);
    comboTxt = Sprite::create(); comboRing = Sprite::create();
    addChild(comboRing, 5004); addChild(comboTxt, 5005);
    hud = DrawNode::create(); addChild(hud, 5006);

    lbl = Label::createWithSystemFont("", "Arial", 16);
    lbl->setAnchorPoint(Vec2(1, 1)); lbl->setPosition(wheelC().x - 78, org.y + vs.height - 8);
    addChild(lbl, 5001);
    lblTitle = Label::createWithSystemFont("", "Arial", 16);
    lblTitle->setAnchorPoint(Vec2(0, 1)); lblTitle->setPosition(org.x + 84, org.y + vs.height - 8);
    addChild(lblTitle, 5001);
    lblCombo = Label::createWithSystemFont("", "Arial", 20);
    lblCombo->setPosition(cx, org.y + vs.height - 40);
    addChild(lblCombo, 5006);
    lblOver = Label::createWithSystemFont("", "Arial", 22);
    lblOver->setPosition(cx, org.y + vs.height / 2);
    addChild(lblOver, 5009);

    auto l = EventListenerTouchAllAtOnce::create();
    l->onTouchesBegan = [this](const std::vector<Touch*>& ts, Event*) {
        for (auto t : ts) {
            Vec2 p = t->getLocation();
            if ((p - pausePos()).length() < 24.f) { paused = !paused; continue; }
            if (paused) continue;
            if ((p - abilPos()).length() < 34.f) { useAbility(); continue; }
            bool onPort = false;
            for (int i = 0; i < 3; i++) {
                if (i != act && (p - portPos(i)).length() < 30.f) { switchTo(i); onPort = true; break; }
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
            if (t->getID() == atkId) { float hold = T - atkStart; atkId = -1; if (!paused) attack(hold >= .8f); }
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
    for (int i = 0; i < 3; i++) { hhp[i] = 6; abilT[i] = 8; abilReady[i] = false; }
    act = (L.startChar == "Darwin") ? H_DAR : (L.startChar == "Anais") ? H_ANA : H_GUM;
    pf = 1; pst = 0; pt = 0; score = 0; pinv = 0; plastAtk = -9; cb = 0;
    chg = false; levelT = 0; phase = 0; spawnT = .6f;
    rushT = 0; shieldT = 0; healFx = 0; combo = 0; comboT = 0; paused = false;
    joyId = -1; joyV = Vec2::ZERO; atkId = -1;
}

void GameLayer::switchTo(int i) {
    if (phase != 0 || i == act || hhp[i] <= 0) return;
    act = i; pst = 0; pt = 0; chg = false; cb = 0; atkId = -1;
    sfx("audio/sfx_gui_wheel_tap.ogg", .6f);
}

void GameLayer::useAbility() {
    if (phase != 0 || abilT[act] > 0) return;
    abilT[act] = 14.f; abilReady[act] = false;
    sfx("audio/sfx_gui_abilitypress.ogg", .6f);
    if (act == H_GUM) { rushT = 6.f; sfx("audio/sfx_char_gumball_sugarrush.ogg", .7f); }
    else if (act == H_DAR) { shieldT = 5.f; sfx("audio/sfx_char_darwin_bubbleshield_on.ogg", .7f); }
    else {
        for (int i = 0; i < 3; i++) if (hhp[i] > 0) hhp[i] = std::min(6, hhp[i] + 3);
        healFx = .8f; sfx("audio/sfx_char_anais_heal.ogg", .7f);
    }
}

void GameLayer::landHit() {
    combo++; comboT = 2.5f;
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
    if (charged) { cb = 3; chg = true;
