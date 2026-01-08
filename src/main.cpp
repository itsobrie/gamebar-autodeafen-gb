#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <TlHelp32.h>

#include <algorithm> // std::clamp

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/utils/string.hpp>

#include <cocos2d.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/TextInput.hpp>


using namespace geode;
using namespace geode::prelude;
using namespace cocos2d;


// =======================
// GLOBAL STATE
// =======================

static bool gMuted = false;
static bool gMutedThisAttempt = false;
static int gSpawnPercent = -1;
static bool gAllowDeafenThisAttempt = true;
static bool gStartedFromZero = true;
static bool gIsStartPosAttempt = false;
static bool gWasPracticeMode = false;


static bool isUsingStartPos(PlayLayer* pl) {
    if (!pl) return false;
    auto player = pl->m_player1;
    if (!player) return false;


    return player->getPositionX() > 15.0f;
}

void setXboxMute(bool mute);

static void handlePracticeModeTransition(PlayLayer* pl) {
    if (!pl) return;

    bool isPractice = pl->m_isPracticeMode;
    bool allowPractice =
        Mod::get()->getSettingValue<bool>("Enabled in Practice Mode");

    if (!gWasPracticeMode && isPractice) {
        if (!allowPractice) {
            gAllowDeafenThisAttempt = false;

            if (gMuted || gMutedThisAttempt) {
                setXboxMute(false);
                gMutedThisAttempt = false;
            }
        }
    }

    gWasPracticeMode = isPractice;
}


// =======================
// SETTINGS HELPERS
// =======================

bool isModEnabled() {
    return Mod::get()->getSettingValue<bool>("Enabled by Default");
}



int getDeafenPercent() {
    int d = static_cast<int>(
        Mod::get()->getSettingValue<int64_t>("Deafen Percent")
    );
    return std::clamp(d, 0, 100);
}




int getUndeafenPercent() {
    int u = static_cast<int>(
        Mod::get()->getSettingValue<int64_t>("Undeafen Percent")
    );
    return std::clamp(u, 0, 100);
}

// =======================
// XBOX AUDIO MUTE LOGIC
// =======================


DWORD getProcessId(const char* name) {
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }




    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                DWORD pid = pe.th32ProcessID;
                CloseHandle(snap);
                return pid;
            }
        } while (Process32Next(snap, &pe));
    }




    CloseHandle(snap);
    return 0;
}




void setXboxMute(bool mute) {
    if (gMuted == mute) {
        return;
    }




    DWORD pid = getProcessId("XboxPartyChatHost.exe");
    if (!pid) {
        log::info("Gamebar Autodeafen: XboxPartyChatHost.exe not found; cannot change mute.");
        gMuted = false;
        return;
    }




    log::info("Gamebar Autodeafen: setXboxMute -> {}", mute ? "MUTE" : "UNMUTE");




    HRESULT hr = CoInitialize(nullptr);
    bool comInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;




    IMMDeviceEnumerator* e = nullptr;
    IMMDevice* d = nullptr;
    IAudioSessionManager2* sm = nullptr;
    IAudioSessionEnumerator* se = nullptr;




    do {
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e))) || !e) break;
        if (FAILED(e->GetDefaultAudioEndpoint(eRender, eConsole, &d)) || !d) break;
        if (FAILED(d->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sm)) || !sm) break;
        if (FAILED(sm->GetSessionEnumerator(&se)) || !se) break;




        int count = 0;
        se->GetCount(&count);




        for (int i = 0; i < count; i++) {
            IAudioSessionControl* c = nullptr;
            IAudioSessionControl2* c2 = nullptr;




            if (FAILED(se->GetSession(i, &c)) || !c) continue;
            if (FAILED(c->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&c2)) || !c2) {
                c->Release();
                continue;
            }




            DWORD spid = 0;
            c2->GetProcessId(&spid);
            if (spid == pid) {
                ISimpleAudioVolume* vol = nullptr;
                if (SUCCEEDED(c2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol)) && vol) {
                    vol->SetMute(mute, nullptr);
                    vol->Release();
                    gMuted = mute;
                }
            }




            c2->Release();
            c->Release();
        }
    } while (false);




    if (se) se->Release();
    if (sm) sm->Release();
    if (d) d->Release();
    if (e) e->Release();




    if (comInit) CoUninitialize();
}




// =======================
// STARTPOS / PRACTICE GATE
// =======================




static bool shouldAllowDeafen(PlayLayer* pl) {
    if (!pl) return true;


    if (pl->m_isPracticeMode) {
        return Mod::get()->getSettingValue<bool>("Enabled in Practice Mode");
    }


    return true;
}


// =======================
// PERCENT → MUTE LOGIC
// =======================


void updateMuteFromPercent(int percent) {
    auto pl = PlayLayer::get();
    if (pl) {
        bool allowPractice =
            Mod::get()->getSettingValue<bool>("Enabled in Practice Mode");

        if (pl->m_isPracticeMode && !allowPractice) {
            if (gMuted || gMutedThisAttempt) {
                setXboxMute(false);
                gMutedThisAttempt = false;
            }
            return;
        }
    }

    if (!gAllowDeafenThisAttempt)
        return;

    int deafenAt = getDeafenPercent();
    int undeafenAt = getUndeafenPercent();

    if (deafenAt <= 0)
        return;

    if (undeafenAt < deafenAt)
        undeafenAt = deafenAt;

    if (!gMutedThisAttempt && percent >= deafenAt && percent < undeafenAt) {
        setXboxMute(true);
        gMutedThisAttempt = true;
    }

    if (gMutedThisAttempt && percent >= undeafenAt) {
        setXboxMute(false);
        gMutedThisAttempt = false;
    }
}

// =======================
// CONFIG POPUP (PAUSE MENU)
// =======================


static geode::TextInput* gDeafenInput = nullptr;
static geode::TextInput* gUndeafenInput = nullptr;
static CCLabelBMFont* gMuteStatusLabel = nullptr;



void updateMuteStatusLabel() {
    if (gMuteStatusLabel) {
        gMuteStatusLabel->setString(gMuted ? "Muted" : "Unmuted");
    }
}




class GamebarConfigLayer : public geode::Popup<std::string const&> {
protected:
    bool setup(std::string const&) override {
        this->setKeyboardEnabled(true);




        CCPoint top = ccp(m_size.width / 2.f, m_size.height - 20.f);




        auto title = CCLabelBMFont::create("Gamebar Autodeafen", "goldFont.fnt");
        title->setScale(0.8f);
        title->setPosition(top);
        m_mainLayer->addChild(title);




        // ---- Deafen ----
        auto deafenLabel = CCLabelBMFont::create("Deafen %", "bigFont.fnt");
        deafenLabel->setScale(0.6f);
        deafenLabel->setAnchorPoint({0.f, 0.5f});
        deafenLabel->setPosition(top + ccp(-120, -60));
        m_mainLayer->addChild(deafenLabel);




        gDeafenInput = geode::TextInput::create(80.f, "%");
        gDeafenInput->setFilter("0123456789");
        gDeafenInput->setMaxCharCount(3);
        gDeafenInput->setScale(0.7f);
        gDeafenInput->setPosition(deafenLabel->getPosition() + ccp(140, 0));
        gDeafenInput->setString(std::to_string(getDeafenPercent()));
        m_mainLayer->addChild(gDeafenInput);




        // ---- Undeafen ----
        auto undeafenLabel = CCLabelBMFont::create("Undeafen %", "bigFont.fnt");
        undeafenLabel->setScale(0.6f);
        undeafenLabel->setAnchorPoint({0.f, 0.5f});
        undeafenLabel->setPosition(top + ccp(-120, -100));
        m_mainLayer->addChild(undeafenLabel);




        gUndeafenInput = geode::TextInput::create(80.f, "%");
        gUndeafenInput->setFilter("0123456789");
        gUndeafenInput->setMaxCharCount(3);
        gUndeafenInput->setScale(0.7f);
        gUndeafenInput->setPosition(undeafenLabel->getPosition() + ccp(140, 0));
        gUndeafenInput->setString(std::to_string(getUndeafenPercent()));
        m_mainLayer->addChild(gUndeafenInput);




        // ---- Mute status ----
        gMuteStatusLabel = CCLabelBMFont::create(gMuted ? "Muted" : "Unmuted", "bigFont.fnt");
        gMuteStatusLabel->setScale(0.6f);
        gMuteStatusLabel->setPosition(top + ccp(0, -140));
        m_mainLayer->addChild(gMuteStatusLabel);




        auto toggle = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Toggle Mute"),
            this,
            menu_selector(GamebarConfigLayer::onToggleMute)
        );




        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        toggle->setPosition(gMuteStatusLabel->getPosition() + ccp(0, -40));
        menu->addChild(toggle);
        m_mainLayer->addChild(menu);




        return true;
    }




    void onClose(CCObject* sender) override {
    auto parse = [](std::string const& s, int fallback) {
    if (s.empty()) return fallback;

    if (auto n = geode::utils::numFromString<int>(s)) {
        return *n; 
    }
    return fallback;
};





    int newDeafen =
        std::clamp(parse(gDeafenInput->getString(), getDeafenPercent()), 0, 100);
    int newUndeafen =
        std::clamp(parse(gUndeafenInput->getString(), getUndeafenPercent()), 0, 100);




    Mod::get()->setSettingValue("Deafen Percent", static_cast<int64_t>(newDeafen));
    Mod::get()->setSettingValue("Undeafen Percent", static_cast<int64_t>(newUndeafen));




    Popup::onClose(sender);
}



    void onToggleMute(CCObject*) {
        setXboxMute(!gMuted);
        updateMuteStatusLabel();
    }




public:
    static void open() {
        auto layer = new GamebarConfigLayer();
        if (layer && layer->initAnchored(320.f, 220.f, "", "GJ_square02.png")) {
            layer->autorelease();
            layer->show();
        }
        else {
            CC_SAFE_DELETE(layer);
        }
    }
};




// =======================
// GEODE HOOKS
// =======================




// PlayLayer hook
class $modify(GamebarPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool p1, bool p2) {
        if (!PlayLayer::init(level, p1, p2)) return false;
        gWasPracticeMode = false;
        gSpawnPercent = -1;
        gIsStartPosAttempt = false;
        gMutedThisAttempt = false;
        setXboxMute(false);
        return true;
    }




   void postUpdate(float dt) {
    PlayLayer::postUpdate(dt);

    if (!isModEnabled()) {
        return;
    }

    int percent = this->getCurrentPercentInt();

    handlePracticeModeTransition(this);
    
if (!gAllowDeafenThisAttempt) {
    return; 
}
   
    if (gSpawnPercent < 0) {
    gSpawnPercent = percent;

    bool allowStartPos   = Mod::get()->getSettingValue<bool>("Enabled in StartPos");
    bool allowPractice   = Mod::get()->getSettingValue<bool>("Enabled in Practice Mode");
    bool isPractice      = this->m_isPracticeMode;

    gIsStartPosAttempt = (!isPractice && isUsingStartPos(this));

    gAllowDeafenThisAttempt = true;

    if (gIsStartPosAttempt && !allowStartPos) {
        gAllowDeafenThisAttempt = false;
    }

    if (isPractice && !allowPractice) {
        gAllowDeafenThisAttempt = false;
    }
}


updateMuteFromPercent(percent);


}


    void resetLevel() {
        PlayLayer::resetLevel();
        gWasPracticeMode = false;
        gIsStartPosAttempt = false;
        gStartedFromZero = true;
        gAllowDeafenThisAttempt = true;
        gSpawnPercent = -1;
        gMutedThisAttempt = false;
        setXboxMute(false);
    }




    void onQuit() {
        PlayLayer::onQuit();
        gWasPracticeMode = false;
        gStartedFromZero = true;
        gAllowDeafenThisAttempt = true;
        gSpawnPercent = -1;
        gMutedThisAttempt = false;
        setXboxMute(false);
    }
};




// PauseLayer hook
class $modify(GamebarPauseLayer, PauseLayer) {
public:
    void customSetup() {
        PauseLayer::customSetup();

        if (!isModEnabled()) {
            return; 
        }


        setXboxMute(false);
        gMutedThisAttempt = false;




        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;




        auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
        cache->addSpriteFramesWithFile("GJ_GameSheet03.plist");
       
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_fxOffBtn_001.png");
        sprite->retain();




        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(GamebarPauseLayer::onOpenGamebarConfig)
        );




        menu->addChild(btn);
        menu->updateLayout();
    }




    void onOpenGamebarConfig(CCObject*) {
        GamebarConfigLayer::open();
    }




    void onResume(CCObject* sender) {
    this->PauseLayer::onResume(sender);

    if (!isModEnabled()) {
        setXboxMute(false);
        return;
    }

    if (auto p = PlayLayer::get()) {
        if (!shouldAllowDeafen(p)) {
            if (gMutedThisAttempt || gMuted) {
                gMutedThisAttempt = false;
                setXboxMute(false);
            }
            return;
        }


        updateMuteFromPercent(p->getCurrentPercentInt());
    }
}




    void onRestart(CCObject* sender) {
        this->PauseLayer::onRestart(sender);
        gMutedThisAttempt = false;
        setXboxMute(false);
    }




    void onRestartFull(CCObject* sender) {
        this->PauseLayer::onRestartFull(sender);
        gMutedThisAttempt = false;
        setXboxMute(false);
    }




    void onQuit(CCObject* sender) {
        this->PauseLayer::onQuit(sender);
        gIsStartPosAttempt = false;
        gMutedThisAttempt = false;
        setXboxMute(false);
    }
};




// PlayerObject hook
class $modify(GamebarPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        gWasPracticeMode = false;
        gIsStartPosAttempt = false;
        gStartedFromZero = true;
        gAllowDeafenThisAttempt = true;
        gSpawnPercent = -1;
        gMutedThisAttempt = false;
        setXboxMute(false);
    }
};
