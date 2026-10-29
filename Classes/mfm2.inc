// ---- parte 2: classe do jogo, init, fases, regras ----
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
    void spawnEnemy(int type, int entry, int side);
    void trySpawn(int idx);
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
    std::vector<int> sSpawned;
    std::vector<float> sLast;
    float rdt = 0;
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
        auto et = m.find("EnemyType");
        if (et != m.end()) {
            for (auto& g : et->second.asValueMap()) {
                const ValueMap& gm = g.second.asValueMap();
                int ty = typeOf(vstr(gm, "SpawnType"));
                if (ty < 0) continue;
                SpawnEntry s;
                s.type = ty;
                s.weight = std::max(.1f, vf(gm, "SpawnWeight", 1));
                s.wait = vf(gm, "SpawnWaitTime", 0);
                s.delay = vf(gm, "SpawnDelayBetween", 5);
                s.cap = std::max(1.f, vf(gm, "SpawnCap", 1));
                s.amount = std::max(1.f, vf(gm, "SpawnAmount", 1));
                s.speed = vf(gm, "SpawnWalkingSpeed", 1);
                std::string pt = vstr(gm, "SpawnPoint");
                s.count = 1;
                const char* nm[6] = {"_one", "_two", "_three", "_four", "_five", "_six"};
                for (int k = 0; k < 6; k++) if (pt.find(nm[k]) != std::string::npos) s.count = k + 1;
                L.entries.push_back(s);
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
        L.ok = (L.dur > 0 && !L.entries.empty() && !L.ranges.empty());
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
    cache->addSpriteFramesWithFile("data/mfm-game-npcicetray-sprites.plist", "textures/mfm-game-npcicetray-sprites.png");
    cache->addSpriteFramesWithFile("data/mfm-game-npcmeatloaf-sprites.plist", "textures/mfm-game-npcmeatloaf-sprites.png");

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
    sSpawned.assign(L.entries.size(), 0); sLast.assign(L.entries.size(), -999.f);
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
    if (charged) { cb = 3; chg = true; }
    else { chg = false; cb = (T - plastAtk < .7f) ? (cb % 3) + 1 : 1; }
    pst = 2; pt = 0; phit = false; plastAtk = T;
    if (act == H_GUM) sfx("audio/sfx_char_gumball_chop.ogg", .6f);
    else if (act == H_ANA) sfx(StringUtils::format("audio/sfx_char_anais_punch%d.ogg", chg ? 5 : cb), .6f);
}

void GameLayer::spawnEnemy(int type, int entry, int side) {
    const Level& L = LV[cur];
    Enemy e;
    e.type = type; e.vis = RT[type].vis; e.entry = entry;
    e.n = Node::create(); e.n->setCascadeOpacityEnabled(true);
    for (int i = 0; i < 6; i++) { e.sp[i] = Sprite::create(); e.n->addChild(e.sp[i]); }
    addChild(e.n);
    e.x = side ? bgW + 30 + rand() % 40 : -30 - rand() % 40;
    e.y = 20 + rand() % (int)std::max(30.f, L.maxY - 40);
    e.hp = (float)RT[type].hp; e.t = 0; e.hurt = 0; e.dir = 1; e.lastHit = -9;
    float sm = (entry >= 0 && entry < (int)L.entries.size()) ? L.entries[entry].speed : 1.f;
    e.spd = TD[e.vis].sp * std::max(.6f, std::min(2.f, sm)) * (.9f + (rand() % 20) / 100.f);
    e.st = 0; e.f = 1; e.did = false; e.pid = -1;
    e.ca = nullptr; e.sRun = e.sAtk = e.sHit = e.sDie = -1;
    if (e.vis >= T_ICETRAY) {
        static const char* ccbFiles[4] = {"data/ccb_GameEnemyIceTray.plist", "data/ccb_GameEnemyTomato.plist",
                                          "data/ccb_GameEnemyMeatloaf.plist", "data/ccb_GameEnemyMeatball.plist"};
        CcbActor* ca = CcbActor::create(ccbFiles[e.vis - T_ICETRAY]);
        if (ca) {
            e.n->addChild(ca);
            e.ca = ca;
            e.sRun = ca->seqBy("walk", "run");
            if (e.sRun < 0) e.sRun = ca->firstSeq();
            e.sAtk = ca->seqBy("attack");
            e.sHit = ca->seqBy("hit");
            e.sDie = ca->seqBy("death");
            ca->play(e.sRun, true);
        } else {
            e.vis = T_BROC;
        }
    }
    E.push_back(e);
}

void GameLayer::trySpawn(int idx) {
    const Level& L = LV[cur];
    const SpawnEntry& s = L.entries[idx];
    if (levelT <= s.wait) return;
    if (levelT - sLast[idx] < s.delay) return;
    if (sSpawned[idx] >= (int)s.amount) return;
    int aliveE = 0;
    for (auto& e : E) if (e.entry == idx && e.st != 2) aliveE++;
    if (aliveE >= (int)s.cap) return;
    int side = rand() % 2;
    for (int k = 0; k < s.count; k++) spawnEnemy(s.type, idx, side);
    sSpawned[idx] += 1; sLast[idx] = levelT;
}

void GameLayer::hitPlayer(int dmg) {
    if (pinv > 0 || phase != 0 || shieldT > 0) return;
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
    e.st = 2; e.t = 0; score += RT[e.type].pts * std::max(1, combo);
    deathSfx(e.vis);
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
        if (!e.did && fabsf(px - e.x) < 45 && fabsf(py - e.y) < 28) { e.did = true; hitPlayer(RT[e.type].dmg); }
        if (e.t > .9f || e.x < 0 || e.x > bgW) { e.st = 4; e.t = 0; }
    } else if (e.st == 4) {
        if (e.t > 1.2f) { e.st = 0; e.t = 0; }
    }
}
// ---- fim da parte 2 ----
