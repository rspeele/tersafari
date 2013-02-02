#ifndef PARSEMESSAGES
#ifdef SERVMODE
struct elimservmode : servmode
#else
struct elimclientmode : clientmode
#endif
{
    struct score
    {
        string team;
        int total;
    };

    vector<score> scores;

    score *lookupscore(const char *team)
    {
        loopv(scores)
        {
            if(!strcmp(scores[i].team, team)) return &scores[i];
        }
        return NULL;
    }
    score &makescore(const char *team)
    {
        score *sc = lookupscore(team);
        if(!sc)
        {
            score &add = scores.add();
            add.total = 0;
            copystring(add.team, team);
            return add;
        }
        else return *sc;
    }
    int getteamscore(const char *team)
    {
        score *sc = lookupscore(team);
        if(sc) return sc->total;
        else return 0;
    }
    void getteamscores(vector<teamscore> &teamscores)
    {
        loopv(scores) teamscores.add(teamscore(scores[i].team, scores[i].total));
    }
    void setup()
    {
        scores.setsize(0);
    }
    void cleanup()
    {
    }
    bool hidefrags()
    {
        return true;
    }
#ifdef SERVMODE
    void endround(const char *winner)
    {
        if(!winner) return;
        score &sc = makescore(winner);
        sc.total++;
        sendf(-1, 1, "ri2s", N_ROUNDSCORE, sc.total, sc.team);
    }
    static void startround()
    {
        loopv(clients)
        {
            if(clients[i]->state.state!=CS_SPECTATOR)
            {
                clients[i]->state.respawn();
                sendspawn(clients[i]);
            }
        }
    }
    bool checkround;
    struct winstate
    {
        bool over;
        const char *winner;
    };
    const winstate winningteam()
    {
        winstate won = { false, NULL };
        const char *aliveteam = NULL;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_ALIVE)
            {
                if(aliveteam)
                {
                    if(strcmp(aliveteam, ci->team)) return won;
                }
                else aliveteam = ci->team;
            }
        }
        won.over = true;
        won.winner = aliveteam;
        return won;
    }
    virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        if(!connecting) return;
        loopv(scores)
        {
            score &sc = scores[i];
            putint(p, N_ROUNDSCORE);
            putint(p, sc.total);
            sendstring(sc.team, p);
        }
    }
    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        checkround = true;
    }
    void died(clientinfo *victim, clientinfo *actor)
    {
        checkround = true;
    }
    bool canspawn(clientinfo *ci, bool connecting)
    {
        return clients.length() <= 2;
    }
    bool canchangeteam(clientinfo *ci, const char *oldteam, const char *newteam)
    {
        return true;
        // only allow two teams?
    }
    void update()
    {
        if(!checkround) return;
        checkround = false;
        winstate won = winningteam();
        if(won.over)
        {
            endround(won.winner);
            sendbroadcastf("round: %s", 5000, won.winner ? won.winner : "draw");
            serverevents::add(&startround, 5000);
        }
    }
    // server interface
        // virtual ~servmode() {}

        // virtual void entergame(clientinfo *ci) {}
        // virtual void leavegame(clientinfo *ci, bool disconnecting = false) {}

        // virtual void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip) {}
        // virtual bool canspawn(clientinfo *ci, bool connecting = false) { return true; }
        // virtual void spawned(clientinfo *ci) {}
        // virtual int fragvalue(clientinfo *victim, clientinfo *actor)
        // {
        //     if(victim==actor || isteam(victim->team, actor->team)) return -1;
        //     return 1;
        // }
        // virtual void died(clientinfo *victim, clientinfo *actor) {}
        // virtual bool canchangeteam(clientinfo *ci, const char *oldteam, const char *newteam) { return true; }
        // virtual void changeteam(clientinfo *ci, const char *oldteam, const char *newteam) {}
        // virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting) {}
        // virtual void update() {}
        // virtual void cleanup() {}
        // virtual void setup() {}
        // virtual void newmap() {}
        // virtual void intermission() {}
        // virtual bool hidefrags() { return false; }
        // virtual int getteamscore(const char *team) { return 0; }
        // virtual void getteamscores(vector<teamscore> &scores) {}
        // virtual bool extinfoteam(const char *team, ucharbuf &p) { return false; }
#else
    // client interface
        // virtual ~clientmode() {}

        // virtual void preload() {}
        // virtual int clipconsole(int w, int h) { return 0; }
        // virtual void drawhud(fpsent *d, int w, int h) {}
        // virtual void rendergame() {}
        // virtual void respawned(fpsent *d) {}
        // virtual void setup() {}
        // virtual void checkitems(fpsent *d) {}
        // virtual int respawnwait(fpsent *d) { return 0; }
        // virtual void pickspawn(fpsent *d) { findplayerspawn(d); }
        // virtual void senditems(packetbuf &p) {}
        // virtual void removeplayer(fpsent *d) {}
        // virtual void gameover() {}
        // virtual bool hidefrags() { return false; }
        // virtual int getteamscore(const char *team) { return 0; }
        // virtual void getteamscores(vector<teamscore> &scores) {}
        // virtual void aifind(fpsent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        // virtual bool aicheck(fpsent *d, ai::aistate &b) { return false; }
        // virtual bool aidefend(fpsent *d, ai::aistate &b) { return false; }
        // virtual bool aipursue(fpsent *d, ai::aistate &b) { return false; }
#endif

};
#elif SERVMODE
#else
case N_ROUNDSCORE:
{
    int score = getint(p);
    getstring(text, p);
    if(p.overread() || !text[0]) break;
    eliminationmode.makescore(text).total = score;
    break;
}
#endif
