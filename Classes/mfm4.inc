// ---- parte 4: leitor das animacoes do editor (arquivos ccb_*.plist) ----
#include <map>

struct CKey { float t; int ease; float a, b, c; std::string s; };
struct CNd {
    int parent;
    std::string cls, df;
    Vec2 pos, anc, sc;
    float rot;
    bool vis, flipX, flipY, hasF;
    int op;
    Color3B col;
    std::map<int, std::map<std::string, std::vector<CKey> > > anim;
    Node* n;
    Sprite* sp;
};
struct CSq { int id; std::string name; float dur; };

static std::map<std::string, ValueMap>& ccbCache() {
    static std::map<std::string, ValueMap> m;
    return m;
}
static float easeF(int e, float x) {
    switch (e) {
        case 2: return x * x * x;
        case 3: return 1.f - powf(1.f - x, 3.f);
        case 4: return x < .5f ? 4.f * x * x * x : 1.f - powf(-2.f * x + 2.f, 3.f) / 2.f;
        default: return x;
    }
}
static void evalPair(const std::vector<CKey>& k, float t, float& a, float& b) {
    if (t <= k[0].t) { a = k[0].a; b = k[0].b; return; }
    for (size_t i = 0; i + 1 < k.size(); i++) {
        if (t >= k[i].t && t < k[i + 1].t) {
            if (k[i + 1].ease == 0) { a = k[i].a; b = k[i].b; return; }
            float x = (t - k[i].t) / std::max(1e-6f, k[i + 1].t - k[i].t);
            x = easeF(k[i + 1].ease, x);
            a = k[i].a + (k[i + 1].a - k[i].a) * x;
            b = k[i].b + (k[i + 1].b - k[i].b) * x;
            return;
        }
    }
    a = k.back().a; b = k.back().b;
}
static const CKey& stepKey(const std::vector<CKey>& k, float t) {
    size_t i = 0;
    for (size_t j = 0; j < k.size(); j++) if (k[j].t <= t + 1e-5f) i = j;
    return k[i];
}
static Vec2 cv2(const ValueMap& m, const char* key) {
    const ValueVector& a = m.at(key).asValueVector();
    return Vec2(a[0].asFloat(), a[1].asFloat());
}

class CcbActor : public Node {
public:
    static CcbActor* create(const std::string& file) {
        CcbActor* a = new (std::nothrow) CcbActor();
        if (a && a->load(file)) { a->autorelease(); return a; }
        CC_SAFE_DELETE(a);
        return nullptr;
    }
    int seqBy(const char* a, const char* b = nullptr) const {
        for (size_t i = 0; i < seqs.size(); i++) {
            if (seqs[i].name.find(a) != std::string::npos) return seqs[i].id;
            if (b && seqs[i].name.find(b) != std::string::npos) return seqs[i].id;
        }
        return -1;
    }
    int firstSeq() const { return seqs.empty() ? 0 : seqs[0].id; }
    int cur() const { return curSeq; }
    float durOf(int id) const {
        for (size_t i = 0; i < seqs.size(); i++) if (seqs[i].id == id) return seqs[i].dur;
        return 0.f;
    }
    void play(int id, bool lp) {
        curSeq = id; t = 0; loop = lp; dur = durOf(id);
        resetBase();
        apply(0.f);
    }
    void tick(float dt) {
        t += dt;
        if (dur > .0001f) {
            if (loop) { while (t >= dur) t -= dur; }
            else if (t > dur) t = dur;
        }
        apply(t);
    }
    Vec2 foot;
private:
    bool load(const std::string& file) {
        std::map<std::string, ValueMap>& cache = ccbCache();
        std::map<std::string, ValueMap>::iterator it = cache.find(file);
        if (it == cache.end()) {
            ValueMap vm = FileUtils::getInstance()->getValueMapFromFile(file);
            if (vm.empty()) return false;
            it = cache.insert(std::make_pair(file, vm)).first;
        }
        const ValueMap& vm = it->second;
        ValueMap::const_iterator sq = vm.find("seqs");
        ValueMap::const_iterator nd = vm.find("nodes");
        ValueMap::const_iterator ft = vm.find("foot");
        if (sq == vm.end() || nd == vm.end()) return false;
        const ValueVector& sv = sq->second.asValueVector();
        for (size_t i = 0; i < sv.size(); i++) {
            const ValueMap& m = sv[i].asValueMap();
            CSq s; s.id = m.at("id").asInt(); s.name = m.at("name").asString(); s.dur = m.at("dur").asFloat();
            seqs.push_back(s);
        }
        foot = Vec2::ZERO;
        if (ft != vm.end()) {
            const ValueVector& fv = ft->second.asValueVector();
            if (fv.size() >= 2) foot = Vec2(fv[0].asFloat(), fv[1].asFloat());
        }
        setCascadeOpacityEnabled(true);
        holder = Node::create();
        holder->setCascadeOpacityEnabled(true);
        holder->setPosition(-foot.x, -foot.y);
        addChild(holder);
        const ValueVector& nv = nd->second.asValueVector();
        for (size_t i = 0; i < nv.size(); i++) {
            const ValueMap& m = nv[i].asValueMap();
            CNd c;
            c.parent = m.at("p").asInt();
            c.cls = m.at("cls").asString();
            c.df = m.at("df").asString();
            c.pos = cv2(m, "pos"); c.anc = cv2(m, "anc"); c.sc = cv2(m, "sc");
            c.rot = m.at("rot").asFloat();
            c.vis = m.at("vis").asBool();
            c.op = m.at("op").asInt();
            const ValueVector& cl = m.at("col").asValueVector();
            c.col = Color3B((GLubyte)cl[0].asInt(), (GLubyte)cl[1].asInt(), (GLubyte)cl[2].asInt());
            const ValueVector& fl = m.at("flip").asValueVector();
            c.flipX = fl[0].asBool(); c.flipY = fl[1].asBool();
            c.hasF = false;
            const ValueMap& am = m.at("anim").asValueMap();
            for (ValueMap::const_iterator sa = am.begin(); sa != am.end(); ++sa) {
                int sid = atoi(sa->first.c_str());
                const ValueMap& props = sa->second.asValueMap();
                for (ValueMap::const_iterator pp = props.begin(); pp != props.end(); ++pp) {
                    const std::string& nm = pp->first;
                    const ValueVector& kvs = pp->second.asValueVector();
                    std::vector<CKey> ks;
                    for (size_t q = 0; q < kvs.size(); q++) {
                        const ValueVector& kk = kvs[q].asValueVector();
                        CKey k; k.t = kk[0].asFloat(); k.ease = 0; k.a = 0; k.b = 0; k.c = 0;
                        if (nm == "displayFrame") k.s = kk[1].asString();
                        else if (nm == "visible") k.a = kk[1].asFloat();
                        else if (nm == "color") { k.a = (float)kk[1].asInt(); k.b = (float)kk[2].asInt(); k.c = (float)kk[3].asInt(); }
                        else if (nm == "position" || nm == "scale") { k.ease = kk[1].asInt(); k.a = kk[2].asFloat(); k.b = kk[3].asFloat(); }
                        else { k.ease = kk[1].asInt(); k.a = kk[2].asFloat(); }
                        ks.push_back(k);
                    }
                    c.anim[sid][nm] = ks;
                }
            }
            if (c.cls == "CCSprite") { c.sp = Sprite::create(); c.n = c.sp; c.sp->setAnchorPoint(c.anc); }
            else { c.sp = nullptr; c.n = Node::create(); }
            c.n->setCascadeOpacityEnabled(true);
            Node* par = (c.parent >= 0 && c.parent < (int)nodes.size()) ? nodes[c.parent].n : holder;
            par->addChild(c.n);
            nodes.push_back(c);
        }
        curSeq = firstSeq(); t = 0; loop = true; dur = durOf(curSeq);
        resetBase();
        return true;
    }
    void resetBase() {
        SpriteFrameCache* fc = SpriteFrameCache::getInstance();
        for (size_t i = 0; i < nodes.size(); i++) {
            CNd& c = nodes[i];
            c.n->setPosition(c.pos);
            c.n->setScale(c.sc.x, c.sc.y);
            c.n->setRotation(c.rot);
            if (c.sp) {
                SpriteFrame* f = c.df.empty() ? nullptr : fc->getSpriteFrameByName(c.df);
                if (f) c.sp->setSpriteFrame(f);
                c.hasF = (f != nullptr);
                c.sp->setOpacity((GLubyte)c.op);
                c.sp->setColor(c.col);
                c.sp->setFlippedX(c.flipX);
                c.sp->setFlippedY(c.flipY);
            }
            c.n->setVisible(c.vis && (c.sp == nullptr || c.hasF));
        }
    }
    void apply(float tm) {
        SpriteFrameCache* fc = SpriteFrameCache::getInstance();
        for (size_t i = 0; i < nodes.size(); i++) {
            CNd& c = nodes[i];
            std::map<int, std::map<std::string, std::vector<CKey> > >::iterator ai = c.anim.find(curSeq);
            if (ai == c.anim.end()) continue;
            bool vis = c.vis;
            for (std::map<std::string, std::vector<CKey> >::iterator pp = ai->second.begin(); pp != ai->second.end(); ++pp) {
                const std::string& nm = pp->first;
                const std::vector<CKey>& k = pp->second;
                if (k.empty()) continue;
                float a = 0, b = 0;
                if (nm == "position") { evalPair(k, tm, a, b); c.n->setPosition(a, b); }
                else if (nm == "scale") { evalPair(k, tm, a, b); c.n->setScale(a, b); }
                else if (nm == "rotation") { evalPair(k, tm, a, b); c.n->setRotation(a); }
                else if (nm == "opacity") { evalPair(k, tm, a, b); if (c.sp) c.sp->setOpacity((GLubyte)std::max(0.f, std::min(255.f, a))); }
                else if (nm == "visible") { vis = stepKey(k, tm).a > .5f; }
                else if (nm == "displayFrame") {
                    if (c.sp) {
                        const CKey& kk = stepKey(k, tm);
                        SpriteFrame* f = kk.s.empty() ? nullptr : fc->getSpriteFrameByName(kk.s);
                        if (f) { c.sp->setSpriteFrame(f); c.hasF = true; } else c.hasF = false;
                    }
                } else if (nm == "color") {
                    if (c.sp) {
                        const CKey& kk = stepKey(k, tm);
                        c.sp->setColor(Color3B((GLubyte)kk.a, (GLubyte)kk.b, (GLubyte)kk.c));
                    }
                }
            }
            c.n->setVisible(vis && (c.sp == nullptr || c.hasF));
        }
    }
    std::vector<CSq> seqs;
    std::vector<CNd> nodes;
    Node* holder = nullptr;
    int curSeq = 0;
    float t = 0, dur = 0;
    bool loop = true;
};
// ---- fim da parte 4 ----
