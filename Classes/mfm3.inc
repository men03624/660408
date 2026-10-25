// ---- parte 3: update e desenho ----
void GameLayer::update(float dt) {
    if (paused) { render(); return; }
    if (dt > .05f) dt = .05f;
    T += dt;
    const Level& L = LV[cur];
    if (phase == 0) {
        levelT += dt;
        Vec2 m = joyV;
        pt += dt;
        if (pinv > 0) pinv -= dt;
        if (rushT > 0) rushT -= dt;
        if (healFx > 0) healFx -= dt;
        if (shieldT > 0) { shieldT -= dt; if (shieldT <= 0) sfx("audio/sfx_char_darwin_bubbleshield_off.ogg", .6f); }
        if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
        for (int i = 0; i < 3; i++) {
            if (abilT[i] > 0) { abilT[i] -= dt; if (abilT[i] <= 0) abilT[i] = 0; }
            if (abilT[i] <= 0 && !abilReady[i]) { abilReady[i] = true; if (i == act) sfx("audio/sfx_gui_abilitycharged.ogg", .6f); }
        }
        float spdMul = (rushT > 0) ? 1.5f : 1.f;
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
                    pr.dmg = chg ? 6 : 2; pr.pierce = chg; pr.id = ++projId;
                    P.push_back(pr);
                    sfx(chg ? "audio/sfx_char_darwin_chargerelease.ogg" : "audio/sfx_char_darwin_shoot.ogg", .6f);
                } else {
                    float base = (act == H_ANA) ? 85.f : 75.f;
                    float r = chg ? base + 45.f : ((cb == 3) ? base + 20.f : base);
                    int dmg;
                    if (chg) dmg = (act == H_GUM) ? 8 : 6;
                    else if (cb == 3) dmg = (act == H_GUM) ? 4 : 2;
                    else dmg = 1;
                    if (rushT > 0) dmg += 2;
                    float kb = chg ? 70.f : ((cb == 3) ? 40.f : 14.f);
                    for (auto& e : E) {
                        if (e.st != 2 && T - e.lastHit >= .0667f && fabsf(e.y - py) < 35 && (e.x - px) * pf > -10 && fabsf(e.x - px) < r) {
                            e.hp -= dmg; e.hurt = .25f; e.lastHit = T; e.x += pf * (e.hp > 20 ? kb * .3f : kb); landHit();
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
                px += m.x * 110 * spdMul * dt; py += m.y * 80 * spdMul * dt;
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
                    if (e.st == 2 || e.pid == p.id || T - e.lastHit < .0667f) continue;
                    if (fabsf(e.x - p.x) < 30 && fabsf(e.y - (p.y - 42)) < 30) {
                        e.pid = p.id; e.hp -= p.dmg; e.hurt = .25f; e.lastHit = T;
                        if (e.hp < 20) e.x += (p.vx > 0) ? 8.f : -8.f;
                        landHit();
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
            spawnT = .1f;
            const Range* r = nullptr;
            for (auto& g : L.ranges) if (levelT >= g.t0 && levelT <= g.t1) r = &g;
            if (!r && !L.ranges.empty()) r = &L.ranges.back();
            if (r && !L.entries.empty() && alive < (int)r->cap && alive < 25) {
                float tot = 0;
                for (auto& s : L.entries) tot += s.weight;
                float v = (rand() % 10000) / 10000.f * tot;
                int idx = 0;
                for (int i = 0; i < (int)L.entries.size(); i++) { v -= L.entries[i].weight; if (v <= 0) { idx = i; break; } idx = i; }
                trySpawn(idx);
            }
        }

        for (auto& e : E) {
            e.t += dt;
            if (e.hurt > 0) e.hurt -= dt;
            if (e.st == 2) continue;
            const TypeDef& d = TD[e.vis];
            float dx = px - e.x, dy = py - e.y;
            if (e.vis == T_CHEESE) { cheese(e, dx, dy, dt); continue; }
            e.f = (dx > 0) ? 1 : -1;
            if (e.st == 0) {
                if (e.hurt <= 0) {
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (fabsf(dx) < d.range && fabsf(dy) < 22) { e.st = 1; e.t = 0; e.did = false; attackSfx(e.vis); }
                    else if (dist > .1f) { e.x += dx / dist * e.spd * dt; e.y += dy / dist * e.spd * .7f * dt; }
                }
            } else if (e.st == 1) {
                if (!e.did && e.t > d.hitT) {
                    e.did = true;
                    if (fabsf(px - e.x) < d.range + 13 && fabsf(py - e.y) < 30) hitPlayer(RT[e.type].dmg);
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
    if (healFx > 0) pn->setColor(Color3B(255, 210, 230));
    else if (rushT > 0) pn->setColor(((int)(T * 10) % 2) ? Color3B(255, 160, 210) : Color3B(255, 210, 120));
    else if (held >= .8f) pn->setColor(((int)(T * 12) % 2) ? Color3B(255, 255, 90) : Color3B(255, 190, 40));
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
        const TypeDef& d = TD[e.vis];
        const EF& f = EFR[e.vis];
        float es = ws * persp(e.y) * d.scale;
        Vec2 q = toScr(e.x, e.y);
        e.n->setPosition(q.x, q.y + d.foot * es);
        e.n->setScaleX(e.f * es); e.n->setScaleY(es);
        e.n->setLocalZOrder(1000 - (int)q.y);
        e.n->setOpacity(255);
        for (int i = 0; i < 6; i++) e.sp[i]->setVisible(false);
        if (e.st == 2) {
            e.n->setOpacity((GLubyte)(255 * std::max(0.f, 1.f - e.t / 1.2f)));
            if (e.vis == T_BREAD) setF(e.sp[2], f.s[4]);
            else if (e.vis == T_BROC || e.vis == T_CARROT) setF(e.sp[2], pickN(f.f, e.t / .5f));
            else if (e.vis == T_STEAK) setF(e.sp[0], f.s[0]);
            else setF(e.sp[0], pickN(f.f, e.t / .5f));
            continue;
        }
        if (e.vis == T_BREAD) {
            if (e.st == 1) {
                setF(e.sp[0], frm(e.t < .3f ? "npc_breadman_attack_legs1.png" : "npc_breadman_attack_legs2.png"));
                setF(e.sp[3], pick(f.b, e.t, false));
            } else {
                setF(e.sp[0], pick(f.a, e.t, true)); setF(e.sp[1], f.s[2]);
                setF(e.sp[2], f.s[0]); setF(e.sp[3], f.s[1]); setF(e.sp[5], f.s[3]);
            }
        } else if (e.vis == T_BROC || e.vis == T_CARROT) {
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
        } else if (e.vis == T_STEAK) {
            if (e.st == 1) { setF(e.sp[0], pickN(f.b, e.t / d.dur)); setF(e.sp[1], pickN(f.c, e.t / d.dur)); }
            else setF(e.sp[0], pick(f.a, e.t, true));
        } else {
            if (e.st == 3) setF(e.sp[0], pick(f.b, e.t, true));
            else if (e.st == 4) setF(e.sp[0], pick(f.c, e.t, true));
            else setF(e.sp[0], f.s[0]);
        }
    }

    /* roda de personagens (canto superior direito) */
    for (int i = 0; i < 3; i++) {
        int h = hhp[i];
        int lv = (h >= 5) ? 6 : (h >= 3) ? 4 : (h >= 1) ? 2 : 0;
        bool isAct = (i == act);
        setF(port[i], frm(StringUtils::format("gui_character_%s_health%d.png", HN[i], lv)));
        port[i]->setPosition(portPos(i));
        port[i]->setScale(isAct ? 1.3f : .85f);
        port[i]->setOpacity(h <= 0 ? 90 : (isAct ? 255 : 200));
        port[i]->setLocalZOrder(isAct ? 5006 : 5003);
        if (h <= 0) { setF(fnt[i], frm("gui_character_fainted.png")); fnt[i]->setPosition(portPos(i)); fnt[i]->setScale(.8f); }
        else fnt[i]->setVisible(false);
    }
    Vec2 wc = wheelC();
    setF(heartsSpr, frm(StringUtils::format("gui_character_hearts_%d.png", hhp[act])));
    heartsSpr->setPosition(wc.x, wc.y - 50); heartsSpr->setScale(.95f);
    setF(pauseBtn, frm("gui_button_pause.png"));
    pauseBtn->setPosition(pausePos());

    /* botao de habilidade (canto superior esquerdo) */
    Vec2 ap = abilPos();
    setF(abilBtn, frm(ABIL[act]));
    abilBtn->setPosition(ap);
    bool ready = (abilT[act] <= 0);
    if (ready) { abilBtn->setOpacity(255); abilBtn->setScale(1.f + .06f * sinf(T * 8)); }
    else { abilBtn->setOpacity(110); abilBtn->setScale(1.f); }

    hud->clear();
    if (!ready) {
        float frac = 1.f - abilT[act] / 14.f;
        hud->drawSolidRect(Vec2(ap.x - 28, ap.y - 40), Vec2(ap.x + 28, ap.y - 35), Color4F(0, 0, 0, .5f));
        hud->drawSolidRect(Vec2(ap.x - 28, ap.y - 40), Vec2(ap.x - 28 + 56 * frac, ap.y - 35), Color4F(1, .85f, .2f, 1));
    }
    if (shieldT > 0) {
        float r = 58 * ps_;
        Vec2 c(sp.x, sp.y + 50 * ps_);
        hud->drawSolidCircle(c, r, 0, 28, Color4F(.4f, .7f, 1.f, .22f));
        hud->drawCircle(c, r, 0, 28, false, Color4F(.6f, .85f, 1.f, .8f));
    }

    /* combo (centro, em cima) */
    if (combo >= 1 && comboT > 0) {
        setF(comboRing, frm("gui_combo_innerring.png")); comboRing->setPosition(cx, org.y + vs.height - 40); comboRing->setScale(.8f);
        setF(comboTxt, frm("gui_combo_combotext.png")); comboTxt->setPosition(cx, org.y + vs.height - 14); comboTxt->setScale(.8f);
        lblCombo->setString(StringUtils::format("%d", combo));
    } else {
        comboRing->setVisible(false); comboTxt->setVisible(false); lblCombo->setString("");
    }

    int remain = std::max(0, (int)ceilf(L.dur - levelT));
    lblTitle->setString(StringUtils::format("Fase %d-%d  Tempo %d", L.ch, L.no, remain));
    lbl->setString(StringUtils::format("Pontos %d", score));
    if (paused) lblOver->setString("Pausado\nToque na pausa para continuar");
    else if (phase == 1) lblOver->setString(StringUtils::format("Fase concluida!\nPontos %d\nToque para continuar", score));
    else if (phase == 2) lblOver->setString("Fim de jogo\nToque para tentar de novo");
    else lblOver->setString("");
}
// ---- fim da parte 3 ----
