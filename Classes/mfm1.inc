// ---- parte 1: auxiliares, inimigos, personagens, estruturas ----
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
enum { T_BREAD, T_BROC, T_CARROT, T_STEAK, T_CHEESE, T_ICETRAY, T_TOMATO, T_MEATLOAF, T_MEATBALL, T_N };
struct TypeDef { const char* key; float hp, sp, dmg, range, hitT, dur, scale, foot; };
static const TypeDef TD[T_N] = {
    {"BreadMan", 5, 35, 2, 62, .36f, .65f, .5f, 62},
    {"Broccoli", 3, 38, 1, 58, .45f, .85f, 1.f, 60},
    {"Carrot",   2, 55, 1, 55, .40f, .80f, 1.f, 60},
    {"Steak",    1, 75, 1, 40, .35f, .60f, 1.f, 25},
    {"Cheese",   3, 40, 2,  0,   0,    0, 1.f, 22},
    {"IceTray",  10, 40, 1, 60, .35f, .6f, 1.f, 0},
    {"Tomato",   12, 55, 1, 55, .9f, 1.73f, 1.f, 0},
    {"Meatloaf", 80, 30, 4, 85, .8f, 1.53f, 1.f, 0},
    {"Meatball",  3, 80, 1, 45, .4f, .83f, 1.f, 0}};

/* tipos reais do jogo original: vida, dano (em meios coracoes), pontos e qual animacao usar */
struct RealType { const char* key; int vis, hp, dmg, pts; };
static const RealType RT[10] = {
    {"Broccoli", T_BROC, 9, 1, 5},
    {"Carrot", T_CARROT, 8, 1, 5},
    {"Steak", T_STEAK, 1, 1, 2},
    {"Cheese", T_CHEESE, 3, 1, 8},
    {"BreadMan", T_BREAD, 120, 3, 50},
    {"IceTray", T_ICETRAY, 10, 1, 10},
    {"Meatloaf", T_MEATLOAF, 80, 4, 60},
    {"Meatball", T_MEATBALL, 3, 1, 0},
    {"Tomato", T_TOMATO, 12, 1, 5},
    {"Fridge", T_BREAD, 280, 3, 500}};
static int typeOf(const std::string& n) {
    for (int i = 0; i < 10; i++) if (n == RT[i].key || n == std::string(RT[i].key) + "Wheel") return i;
    return -1;
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

/* ---------- fases e estruturas ---------- */
struct Range { float t0, t1, rate, cap; };
struct SpawnEntry {
    int type, count;
    float weight, wait, delay, cap, amount, speed;
};
struct Level {
    std::string key, bg, startChar;
    int ch, no;
    float dur, minY, maxY, down;
    Vec2 start;
    std::vector<SpawnEntry> entries;
    std::vector<Range> ranges;
    bool ok;
    Level() : ch(0), no(0), dur(0), minY(0), maxY(260), down(.25f), ok(false) {}
};

class CcbActor;
struct Enemy {
    int type, vis, entry;
    CcbActor* ca;
    int sRun, sAtk, sHit, sDie;
    Node* n; Sprite* sp[6];
    float x, y, t, hp, hurt, spd, dir, lastHit;
    int st, f, pid; bool did;   // st: 0 anda, 1 ataca, 2 morto, 3 rola, 4 tonto, 5 prepara
};

struct Proj {
    Sprite* s;
    float x, y, vx, life, t, sc;
    int dmg, id; bool pierce;
};
// ---- fim da parte 1 ----
