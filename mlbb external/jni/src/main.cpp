#include "main.h"
#include <linux/input.h>
#include <linux/uinput.h>
#include <vector>
#include <functional>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <fstream>
#include <cstring>
#include <ctime>
#include <malloc.h>
#include <iostream>
#include <sys/system_properties.h>
#include <string>
#include <exception>
#include <sstream>
#include <thread>
#include <algorithm>

#include "Memory/Memory.h"
#include "Memory/PatternScanner.h"

#include "Quaternion.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"

#include "Includes/Log.h"
#include "Includes/Offset.h"
#include "Engine/CanvasView.h"
#include "include.h"
#include "Matrix4x4.hpp"
#include "ToString.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "icon/HeroIcons.h"
#include "Decoder64.h"
#include "DrawIconHero.h"

using namespace Memory;

// =================================================================
// [ГОССЕН АВТО-КОМБО ЖАНА САКТОО ӨЗГӨРМӨЛӨРҮ]
// =================================================================
bool gusion_combo_enabled = false;   
bool gusion_combo_running = false;   
bool gusion_lock_position = false;   

// Скиллдердин экрандагы координаттары (Sliders аркылуу өзгөрөт)
float g_skill1_x = 1450.0f, g_skill1_y = 850.0f;
float g_skill2_x = 1600.0f, g_skill2_y = 720.0f;
float g_skill3_x = 1750.0f, g_skill3_y = 580.0f;

// Калкып жүрүүчү Комбо Баскычынын (Floating Icon) орду жана өлчөмү
float combo_icon_x = 500.0f;        
float combo_icon_y = 500.0f;        
float combo_icon_size = 65.0f;      

const char* meliora_config_file = "/data/local/tmp/meliora_config.txt";

void ExecuteGusionUltraCombo() {
    // Комбо аткаруу логикасы
}

void SaveConfiguration() {
    std::ofstream outfile(meliora_config_file);
    if (outfile.is_open()) {
        outfile << g_skill1_x << "\n" << g_skill1_y << "\n";
        outfile << g_skill2_x << "\n" << g_skill2_y << "\n";
        outfile << g_skill3_x << "\n" << g_skill3_y << "\n";
        outfile << combo_icon_x << "\n" << combo_icon_y << "\n";
        outfile << combo_icon_size << "\n";
        outfile << gusion_lock_position << "\n";
        outfile.close();
    }
}

void LoadConfiguration() {
    std::ifstream infile(meliora_config_file);
    if (infile.is_open()) {
        infile >> g_skill1_x >> g_skill1_y;
        infile >> g_skill2_x >> g_skill2_y;
        infile >> g_skill3_x >> g_skill3_y;
        infile >> combo_icon_x >> combo_icon_y;
        infile >> combo_icon_size;
        infile >> gusion_lock_position;
        infile.close();
    } else {
        SaveConfiguration(); 
    }
}
// =================================================================

bool main_thread_flag = true;
int abs_ScreenX = 0;
int abs_ScreenY = 0;
bool drawMAddress;
bool drawMBox = true;
bool drawMLine = true;
bool drawMPostion = true;
bool drawMHealth = true;
bool drawMDistance = true;
bool drawMName = true;
bool drawAlertUnderAttack = true;
bool iconhero = true;
float RadiusCir = 50.0f;
long libbase = 0;

std::string fshy(uintptr_t address)
{
    if (!address) return "";

    auto stringLength = Read<uint32_t>(address + 0x10); // STR_LENGTH = 0x10
    char16_t buffer[255] = { 0 };

    pvm(reinterpret_cast<void *>(address + 0x14), reinterpret_cast<void *>(buffer), static_cast<size_t>(stringLength) * 2, false); // STR_BUFFER = 0x14

    return utf16_to_utf8(buffer, stringLength);
}

struct Camera {
    Matrix4x4 worldToCameraMatrix;
    Matrix4x4 projectionMatrix;
};

Matrix4x4 _vMatrix;

// Жаңы кадамдык Нативдик Камера табуу (v2.2.16.x)
uintptr_t GetMainCamera() {
    auto game_method = Read<uintptr_t>(libbase + 0x635b620); // GameMethod class
    if (!game_method) return 0;
    
    auto static_fields = Read<uintptr_t>(game_method + 0xa8); // IL2CPP_STATIC_FIELDS_OFFSET
    if (!static_fields) return 0;
    
    auto smooth_follow = Read<uintptr_t>(static_fields + 0x10); // SmoothFollow
    if (!smooth_follow) return 0;

    auto main_cam = Read<uintptr_t>(smooth_follow + 0x48); // m_MainCamera
    if (!main_cam) return 0;

    auto native_cam = Read<uintptr_t>(main_cam + 0x10); // native Camera
    if (!native_cam) return 0;

    return native_cam;
}

bool WorldToScreen(Vector3 from, Vector2 *to) {
    auto viewMatrix = _vMatrix.MultiplyPoint(from);
    auto screenPos = Vector3(viewMatrix.X + 1.0f, viewMatrix.Y + 1.0f, viewMatrix.Z + 1.0f) / 2.0f;
    *to = Vector2(screenPos.X * abs_ScreenX, abs_ScreenY - (screenPos.Y * abs_ScreenY));
    return viewMatrix != Vector3::Zero();
}

void FindPoint(Vector2 origin, Vector2 &point, int screenwidth, int screenheight, int length)
{
    float halfScreenWidth = screenwidth / 2.0f;
    float halfScreenHeight = screenheight / 2.0f;
    float halfScreenWidth2 = (screenwidth - length) / 2.0f;
    float halfScreenHeight2 = (screenheight - length) / 2.0f;
    float dx = fabs(origin.X - halfScreenWidth);
    float dy = fabs(origin.Y - halfScreenHeight);
    float rx = (dx != 0) ? halfScreenWidth2 / dx : 0;
    float ry = (dy != 0) ? halfScreenHeight2 / dy : 0;
    float r = fmin(rx, ry);
    point.X = origin.X + (halfScreenWidth - origin.X) * (1.0f - r);
    point.Y = origin.Y + (halfScreenHeight - origin.Y) * (1.0f - r);
}

int ListMonsterId[] = {
    2002, 2003, 2004, 2005, 2006, 2008, 2009, 2011, 2012, 2013,
    2056, 2059, 2072, 2220, 2221, 2222, 2223, 2224, 2225, 2226,
    2227, 2228, 2229, 2230, 2232
};

bool bMonster(int iValue) {
    return std::find(std::begin(ListMonsterId), std::end(ListMonsterId), iValue) != std::end(ListMonsterId);
}

void Touch_Tap(int x, int y) {
     Touch_Down((float)x, (float)y);
     usleep(80000);
     Touch_Up();
}

bool lastRetriTriggered[20] = {false};
bool autoRetribution = false;
bool AutoRetributionRed = false;
bool AutoRetributionBlue = false;
bool AutoRetributionLord = false;
bool AutoRetributionTurtle = false;
bool AutoRetributionCrab = false;
bool AutoRetributionLito = false;        

float retriTouchX = 1575.0f;
float retriTouchY = 661.0f;

void DrawMonster(ImDrawList *Draw) {
    if (autoRetribution) {
        ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(retriTouchX, retriTouchY), 18.0f, IM_COL32(255, 255, 255, 180), 16);
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(retriTouchX, retriTouchY), 18.0f, IM_COL32(0, 0, 0, 255), 16, 2.5f);
    }
    if (abs_ScreenX < abs_ScreenY) return;
    
    float lineSize = abs_ScreenY / 432.0f;

    // Жаңы BattleManager чынжырчасы (v2.2.16.x)
    uintptr_t battle_class = Read<uintptr_t>(libbase + 0x635b290);
    if (!battle_class) return;
    uintptr_t static_fields = Read<uintptr_t>(battle_class + 0xa8);
    if (!static_fields) return;
    uintptr_t a32 = Read<uintptr_t>(static_fields + 0x0); // BattleManager.Instance
    if (!a32) return;

    size_t m_LocalPlayerShow = 0x48;
    size_t m_ShowPlayers     = 0x70;
    size_t m_ShowMonsters    = 0x78;
    
    size_t m_ID              = 0x18c;
    size_t m_iType          = 0x78;
    size_t m_bDeath         = 0xc5;
    size_t m_Hp             = 0x1a4;
    size_t m_HpMax          = 0x1a8;
    size_t m_vCachePosition = 0x28c;
    size_t m_bSameCampType  = 0x2a9;
    size_t m_HeroName       = 0x8f0;
    
    uintptr_t selfp = Read<uintptr_t>(a32 + m_LocalPlayerShow);
    
    auto camera = GetMainCamera();
    if (!camera) return;
    
    auto ViewMatrix = Read<Camera>(camera + 0x5C);
    _vMatrix = ViewMatrix.projectionMatrix * ViewMatrix.worldToCameraMatrix;

    uintptr_t showPlayersPtr = Read<uintptr_t>(a32 + m_ShowPlayers);
    if (!showPlayersPtr) return;

    uintptr_t playerList = Read<uintptr_t>(showPlayersPtr + 0x10); // LIST_ITEMS = 0x10
    if (!playerList) return;
    playerList += 0x20; // LIST_DATA_OFF = 0x20

    uint stop_player = Read<uint>(showPlayersPtr + 0x18); // LIST_SIZE = 0x18
    
    for (uint i = 0; i < stop_player; i++) {
        auto Objaddr = Read<uintptr_t>(playerList + (i << 3));

        if (!Objaddr) continue;

        auto is_team = Read<bool>(Objaddr + m_bSameCampType);
        if (is_team) continue;

        auto HeroID = Read<int>(Objaddr + m_ID);     

        auto death = Read<bool>(Objaddr + m_bDeath);
        if (death) continue;

        int Health = Read<int>(Objaddr + m_Hp);
        if (Health <= 0) continue;

        int maxHealth = Read<int>(Objaddr + m_HpMax);
        if (maxHealth <= 0) continue;

        Vector3 Z{0, 0, 0};
        if (selfp) vm_readv(selfp + m_vCachePosition, &Z, sizeof(Z));
      
        Vector3 D{0, 0, 0};
        vm_readv(Objaddr + m_vCachePosition, &D, sizeof(D));
        
        Vector2 en_posSc;
        WorldToScreen(D, &en_posSc);
        
        Vector2 loc_posSc;
        WorldToScreen(Z, &loc_posSc);
        
        Vector2 HeroPos = {en_posSc.X, en_posSc.Y};
        
        auto Distance = Vector3::Distance(Z, D);
    
        if (drawMHealth) {
            ImGui::GetForegroundDrawList()->AddLine({loc_posSc.X, loc_posSc.Y}, {en_posSc.X, en_posSc.Y}, ImColor(255, 255, 255), lineSize);
        }
        
        if (iconhero) {
            ImVec2 iconPos(HeroPos.X, HeroPos.Y);
            DrawHeroIcon(ImGui::GetBackgroundDrawList(), iconPos, HeroID, Health, maxHealth);
        }

        if (drawMDistance) {
            std::string s;
            s += std::to_string((int)Distance);
            s += "m | ";
            s += "Health: " + std::to_string((int)Health);
            uintptr_t namePtr = Read<uintptr_t>(Objaddr + m_HeroName);
            if (namePtr) s += " | " + fshy(namePtr);

            auto textSize1 = ImGui::CalcTextSize(s.c_str(), 0, 29);
            绘制字体描边(22.5, HeroPos.X - (textSize1.x / 2), HeroPos.Y, ImColor(248, 248, 255), s.c_str());
        }
    }

    uintptr_t showMonstersPtr = Read<uintptr_t>(a32 + m_ShowMonsters);
    if (!showMonstersPtr) return;

    uintptr_t monsterList = Read<uintptr_t>(showMonstersPtr + 0x10);
    if (!monsterList) return;
    monsterList += 0x20;

    uint stop_monster = Read<uint>(showMonstersPtr + 0x18);
    
    for (uint i = 0; i < stop_monster; i++) {
        auto Objaddr = Read<uintptr_t>(monsterList + (i << 3));

        if (!Objaddr) continue;

        auto is_team = Read<bool>(Objaddr + m_bSameCampType);
        if (is_team) continue;

        auto mHeroID = Read<int>(Objaddr + m_ID);        
        auto type = Read<int>(Objaddr + m_iType);
        
        auto death = Read<bool>(Objaddr + m_bDeath);
        if (death) continue;

        int Health = Read<int>(Objaddr + m_Hp);
        if (Health <= 0) continue;
        
        int maxHealth = Read<int>(Objaddr + m_HpMax);
        if (maxHealth <= 0) continue;
        
        Vector3 ZL{0, 0, 0};
        if (selfp) vm_readv(selfp + m_vCachePosition, &ZL, sizeof(ZL));
      
        Vector3 Dm{0, 0, 0};
        vm_readv(Objaddr + m_vCachePosition, &Dm, sizeof(Dm));
        
        Vector2 mon_posSc;
        WorldToScreen(Dm, &mon_posSc);
        
        Vector2 MonPos = {mon_posSc.X, mon_posSc.Y};
        if (MonPos.X < 0 || MonPos.X > abs_ScreenX || MonPos.Y < 0 || MonPos.Y > abs_ScreenY) {
            continue;
        }     
        if (type == 5) {           
           if (mHeroID == 2002 && Health < maxHealth) {
               std::string s = "LORD UNDER ATK!";
               std::string h = "Health: " + std::to_string((int)Health);
               Draw->AddText(nullptr, 22.5f, ImVec2(abs_ScreenX / 2.0f - 70.0f, 30), ImColor(248, 248, 255), s.c_str());
               Draw->AddText(nullptr, 22.5f, ImVec2(abs_ScreenX / 2.0f - 70.0f, 50), ImColor(248, 248, 255), h.c_str());               
           }
        
           if (mHeroID == 2003 && Health < maxHealth) {
               std::string s = "TURTLE UNDER ATK!";
               std::string h = "Health: " + std::to_string((int)Health);

               Draw->AddText(nullptr, 22.5f, ImVec2(abs_ScreenX / 2.0f - 70.0f, 30), ImColor(248, 248, 255), s.c_str());
               Draw->AddText(nullptr, 22.5f, ImVec2(abs_ScreenX / 2.0f - 70.0f, 50), ImColor(248, 248, 255), h.c_str());               
           }
        }
        if (type == 1) {
            std::string sL = "MINION";
            auto textSize1 = ImGui::CalcTextSize(sL.c_str(), 0, 29); 
            绘制字体描边(22.5, MonPos.X - (textSize1.x / 2), MonPos.Y, ImColor(248, 248, 255), sL.c_str());
        }
        if (type == 2) {
             if (!bMonster(mHeroID)) continue;               
           
             std::string monsterName = MonsterToString(mHeroID);
             if (monsterName.empty()) continue;             
           
             bool isEventMonster = (mHeroID >= 2220 && mHeroID <= 2232);
             ImColor nameColor = isEventMonster ? IM_COL32(255, 215, 0, 255) : IM_COL32(220, 180, 255, 255);     
           
             auto DistanceM = Vector3::Distance(ZL, Dm);         
         
             std::string strName = monsterName;
             if (isEventMonster) {
                 strName = "[EVENT] " + monsterName;
             } 
             
             auto textSize1 = ImGui::CalcTextSize(strName.c_str(), 0, 29); 
             绘制字体描边(22.5, MonPos.X - (textSize1.x / 2), MonPos.Y + 20, nameColor, strName.c_str());
             
             std::string sm;     
             sm += std::to_string((int)DistanceM);
             sm += "m | ";
             sm += "Health: " + std::to_string((int)Health);      
             auto textSize11 = ImGui::CalcTextSize(sm.c_str(), 0, 29);    
             绘制字体描边(22.5, MonPos.X - (textSize11.x / 2), MonPos.Y, nameColor, sm.c_str());
        }
    }
}

struct MonsterData {
    uintptr_t address;
    Vector3 position;
    float distance;
    int health;
    int maxHP;
    bool isDead;
    bool isVisible;
    bool isValid;
    char name[100];
};

MonsterData monster[20];
int MonsterCount = 0;
uintptr_t Oneself = 0;

void MonsterRetribution() {
    uintptr_t battle_class = Read<uintptr_t>(libbase + 0x635b290);
    if (!battle_class) return;
    uintptr_t static_fields = Read<uintptr_t>(battle_class + 0xa8);
    if (!static_fields) return;
    uintptr_t BattleManager = Read<uintptr_t>(static_fields + 0x0);
    if (!BattleManager) return;

    Oneself = Read<uintptr_t>(BattleManager + 0x48); // m_LocalPlayerShow = 0x48
    if (!Oneself) return;

    Vector3 MyPosition{0, 0, 0};
    vm_readv(Oneself + 0x28c, &MyPosition, sizeof(MyPosition)); // m_vCachePosition = 0x28c
    
    MonsterCount = 0;
    uintptr_t Showmonster = Read<uintptr_t>(BattleManager + 0x78); // m_ShowMonsters = 0x78
    if (Showmonster != 0) {
        int monsterCount = Read<int>(Showmonster + 0x18);
        uintptr_t monsterDataPtr = ReadPtr(Showmonster + 0x10);
        if (monsterCount >= 0 && monsterCount <= 100 && monsterDataPtr != 0) {
            uintptr_t monsterDataArray = monsterDataPtr + 0x20;
            int monsterfound = 0;
            for (int i = 0; i < monsterCount && monsterfound < 20; i++) {
                uintptr_t currentMonsterPtr = ReadPtr(monsterDataArray + (i * 8));
                if (currentMonsterPtr == 0) continue;
                int monsterID = Read<int>(currentMonsterPtr + 0x18c); // m_ID = 0x18c
                int monsterHP = Read<int>(currentMonsterPtr + 0x1a4); // m_Hp = 0x1a4
                int monsterMaxHP = Read<int>(currentMonsterPtr + 0x1a8); // m_HpMax = 0x1a8
                Vector3 monsterPos{0, 0, 0};
                vm_readv(currentMonsterPtr + 0x28c, &monsterPos, sizeof(monsterPos));
                uint8_t deadFlag = Read<uint8_t>(currentMonsterPtr + 0xc5); // m_bDeath = 0xc5
                bool mDead = (deadFlag != 0);
                std::string mName = MonsterToString(monsterID);
                if (mName.empty()) {
                    if (monsterID == 2002) mName = "Lord";
                    else if (monsterID == 2003) mName = "Turtle";
                    else continue;
                }
                monster[monsterfound].address   = currentMonsterPtr;
                monster[monsterfound].position  = monsterPos;
                monster[monsterfound].distance  = Vector3::Distance(MyPosition, monsterPos);
                monster[monsterfound].health    = monsterHP;
                monster[monsterfound].maxHP     = monsterMaxHP;
                monster[monsterfound].isDead    = mDead;
                monster[monsterfound].isVisible = true;
                monster[monsterfound].isValid   = true;
                strncpy(monster[monsterfound].name, mName.c_str(), sizeof(monster[monsterfound].name) - 1);
                monster[monsterfound].name[sizeof(monster[monsterfound].name) - 1] = '\0';
                monsterfound++;
            }
            MonsterCount = monsterfound;
        }
    }
}

int CalculateRetriDamage(int Level, int KillWild) {
    if (KillWild < 5) {
        return 600 + (Level - 1) * 80;
    } else {
        return (600 + (Level - 1) * 80) + (300 + (Level - 1) * 40);
    }
}

void CheckAndTriggerRetribution() {
    if (!autoRetribution || !Oneself || MonsterCount <= 0) return;
    int myLevel = Read<int>(Oneself + 0x190); // m_Level = 0x190
    int killWild = Read<int>(Oneself + 0xa28); // m_KillWildTimes = 0xa28
    int retriDmg = CalculateRetriDamage(myLevel, killWild);
    for (int i = 0; i < MonsterCount; i++) {
        if (!monster[i].isValid || monster[i].isDead) {
            lastRetriTriggered[i] = false;
            continue;
        }
        if (monster[i].distance > 5.0f) {
            lastRetriTriggered[i] = false;
            continue;
        }
        int id = Read<int>(monster[i].address + 0x18c); // m_ID = 0x18c
        bool isTarget = false;
        if (AutoRetributionLord && (id == 2002)) isTarget = true;
        if (AutoRetributionTurtle && (id == 2003)) isTarget = true;
        if (AutoRetributionBlue && (id == 2005)) isTarget = true;
        if (AutoRetributionLito && (id == 2056)) isTarget = true;
        if (AutoRetributionCrab && (id == 2005)) isTarget = true;
        if (AutoRetributionRed && (id == 2004)) isTarget = true;        
        if (!isTarget) {
            lastRetriTriggered[i] = false;
            continue;
        }
        if (monster[i].health <= retriDmg) {
            if (!lastRetriTriggered[i]) {
                Touch_Tap(retriTouchX, retriTouchY);
                lastRetriTriggered[i] = true;
            }
        } else {
            lastRetriTriggered[i] = false;
        }
    }
}

int MinimapSize = 342;
int MinimapPos = 76;
bool MinimapIcon = true;
bool HideLine = false;

float g_MinimapScale = 74.11f;
float g_Res0_MultX = 1.0f;
float g_Res0_MultY = 1.0f;
float g_Res1_OffsetX = 0.0f;
float g_Res1_OffsetY = 0.0f;
int g_ICSize = 38;

Vector2 WorldToMinimap(Vector3 HeroPosition) {
    float angle = 314.60f * 0.017453292519943295f;
    float angleCos = std::cos(angle);
    float angleSin = std::sin(angle);

    Vector2 Res0;
    Res0.X = ((angleCos * HeroPosition.X - angleSin * (-HeroPosition.Z)) / g_MinimapScale) * g_Res0_MultX;
    Res0.Y = ((angleSin * HeroPosition.Y + angleCos * (-HeroPosition.Z)) / g_MinimapScale) * g_Res0_MultY;

    Vector2 Res1;
    Res1.X = (Res0.X * MinimapSize) + MinimapPos + MinimapSize / 2.0f + g_Res1_OffsetX;
    Res1.Y = (Res0.Y * MinimapSize) + MinimapSize / 2.0f + g_Res1_OffsetY;

    return Res1;
}

void DrawMinimapESP(ImDrawList* draw) {
    if (!MinimapIcon) return;

    uintptr_t battle_class = Read<uintptr_t>(libbase + 0x635b290);
    if (!battle_class) return;
    uintptr_t static_fields = Read<uintptr_t>(battle_class + 0xa8);
    if (!static_fields) return;
    uintptr_t a32 = Read<uintptr_t>(static_fields + 0x0);
    if (!a32) return;

    size_t m_ShowPlayers     = 0x70;
    size_t m_bSameCampType   = 0x2a9;
    size_t m_bDeath          = 0xc5;
    size_t m_vCachePosition  = 0x28c;

    long showList = Read<uintptr_t>(a32 + m_ShowPlayers);
    if (!showList) return;

    long playerList = Read<uintptr_t>(showList + 0x10);
    if (!playerList) return;
    playerList += 0x20;

    uint playerCount = Read<uint>(showList + 0x18);
    for (uint i = 0; i < playerCount; i++) {
        long Objaddr = Read<uintptr_t>(playerList + (i << 3));
        if (!Objaddr) continue;

        if (Read<bool>(Objaddr + m_bSameCampType)) continue;
        if (Read<bool>(Objaddr + m_bDeath)) continue;

        Vector3A pos{};
        vm_readv(Objaddr + m_vCachePosition, &pos, sizeof(pos));
        if (pos.X == 0 && pos.Y == 0 && pos.Z == 0) continue;

        Vector2 minimapPos = WorldToMinimap({ pos.X, pos.Y, pos.Z });
        draw->AddCircleFilled(ImVec2(minimapPos.X, minimapPos.Y), g_ICSize / 2.0f, IM_COL32(255, 0, 0, 255));
    }

    if (!HideLine) {
        draw->AddRect(
            ImVec2(MinimapPos, 0),
            ImVec2(MinimapPos + MinimapSize, MinimapSize),
            IM_COL32(255, 255, 255, 255)
        );
    }
}

void Layout_tick_UI() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 0), ImVec2(820, FLT_MAX));

    ImGui::Begin(oxorany("                  MLBB EXTERNAL - STARCOOL"), nullptr, window_flags);

    if (ImGui::BeginTabBar("####")) {

        if (ImGui::BeginTabItem(oxorany("ESP"))) {
            ImGui::Checkbox(oxorany("Line"), &drawMHealth);
            ImGui::Checkbox(oxorany("IconHero"), &iconhero);
            ImGui::Checkbox(oxorany("Distance & Hero Name"), &drawMDistance);
            ImGui::Checkbox(oxorany("Alert Lord Under Attack"), &drawAlertUnderAttack);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(oxorany("Retri"))) {
            ImGui::Checkbox(oxorany("Auto Retri"), &autoRetribution);
            ImGui::SliderFloat(oxorany("Adjust X"), &retriTouchX, 0.0f, 3000.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Adjust Y"), &retriTouchY, 0.0f, 1500.0f, "%.0f");

            ImGui::Checkbox(oxorany("Buff Red"), &AutoRetributionRed);
            ImGui::Checkbox(oxorany("Buff Blue"), &AutoRetributionBlue);
            ImGui::Checkbox(oxorany("Lord"), &AutoRetributionLord);
            ImGui::Checkbox(oxorany("Turtle"), &AutoRetributionTurtle);
            ImGui::Checkbox(oxorany("Crab"), &AutoRetributionCrab);
            ImGui::Checkbox(oxorany("Lito"), &AutoRetributionLito);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(oxorany("Gusion Matrix"))) {
            ImGui::Spacing();
            ImGui::Checkbox(oxorany("Enable Gusion Auto-Combo"), &gusion_combo_enabled);
            ImGui::Separator();

            if (gusion_combo_enabled) {
                ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), oxorany("Floating Icon Options:"));
                ImGui::Checkbox(oxorany("Lock Icon Position (Катыруу)"), &gusion_lock_position);
                ImGui::SliderFloat(oxorany("Icon Size (Өлчөмү)"), &combo_icon_size, 40.0f, 150.0f, "%.0f");
                ImGui::Separator();
            }

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), oxorany("Skill Button Calibration:"));
            ImGui::SliderFloat(oxorany("Gusion Skill 1 X"), &g_skill1_x, 0.0f, 2500.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Gusion Skill 1 Y"), &g_skill1_y, 0.0f, 1500.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Gusion Skill 2 X"), &g_skill2_x, 0.0f, 2500.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Gusion Skill 2 Y"), &g_skill2_y, 0.0f, 1500.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Gusion Skill 3 X"), &g_skill3_x, 0.0f, 2500.0f, "%.0f");
            ImGui::SliderFloat(oxorany("Gusion Skill 3 Y"), &g_skill3_y, 0.0f, 1500.0f, "%.0f");

            // =========================================================================
            // [КҮРӨҢ ТӨГОЛОК ТОЧКА МЕНЕН СКИЛДЕРДИН ОРДУН ЭКРАНДА КӨРСӨТҮҮ]
            // =========================================================================
            ImDrawList* fg_draw = ImGui::GetForegroundDrawList();

            // Skill 1 Position Dot (Brown / Күрөң түстөгү чекит)
            fg_draw->AddCircleFilled(ImVec2(g_skill1_x, g_skill1_y), 18.0f, IM_COL32(139, 69, 19, 230));
            fg_draw->AddCircle(ImVec2(g_skill1_x, g_skill1_y), 18.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
            fg_draw->AddText(ImVec2(g_skill1_x - 10.0f, g_skill1_y - 8.0f), IM_COL32(255, 255, 255, 255), "S1");

            // Skill 2 Position Dot
            fg_draw->AddCircleFilled(ImVec2(g_skill2_x, g_skill2_y), 18.0f, IM_COL32(139, 69, 19, 230));
            fg_draw->AddCircle(ImVec2(g_skill2_x, g_skill2_y), 18.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
            fg_draw->AddText(ImVec2(g_skill2_x - 10.0f, g_skill2_y - 8.0f), IM_COL32(255, 255, 255, 255), "S2");

            // Skill 3 Position Dot
            fg_draw->AddCircleFilled(ImVec2(g_skill3_x, g_skill3_y), 18.0f, IM_COL32(139, 69, 19, 230));
            fg_draw->AddCircle(ImVec2(g_skill3_x, g_skill3_y), 18.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
            fg_draw->AddText(ImVec2(g_skill3_x - 10.0f, g_skill3_y - 8.0f), IM_COL32(255, 255, 255, 255), "S3");
            // =========================================================================

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(oxorany("SAVE CONFIG TO MEMORY (САКТОО)"), ImVec2(330, 40))) {
                SaveConfiguration(); 
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(oxorany("Minimap"))) {
            if (ImGui::CollapsingHeader("Minimap Setting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Minimap", &MinimapIcon);
                ImGui::SameLine();
                ImGui::Checkbox("Hide Line", &HideLine);
                ImGui::SliderInt("Minimap Size", &MinimapSize, 100, 600);
                ImGui::SliderInt("Minimap Pos X", &MinimapPos, 0, 800);
                ImGui::SliderInt("Size", &g_ICSize, 1, 100);
                ImGui::Text("WorldToMinimap Tweak:");
                ImGui::SliderFloat("Res0 X Mult", &g_Res0_MultX, 0.1f, 3.0f);
                ImGui::SliderFloat("Res0 Y Mult", &g_Res0_MultY, 0.1f, 3.0f);
                ImGui::SliderFloat("Res1 Offset X", &g_Res1_OffsetX, -200.0f, 200.0f);
                ImGui::SliderFloat("Res1 Offset Y", &g_Res1_OffsetY, -200.0f, 200.0f);
                ImGui::SliderFloat("Minimap Scale", &g_MinimapScale, 10.0f, 150.0f);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(oxorany("Settings"))) {
            static int theme = 0;
            const char* themes[] = { "Dark", "Light", "Classic" };
            if (ImGui::Combo(oxorany("Theme Gui"), &theme, themes, IM_ARRAYSIZE(themes))) {
                if (theme == 0) ImGui::StyleColorsDark();
                if (theme == 1) ImGui::StyleColorsLight();
                if (theme == 2) ImGui::StyleColorsClassic();
            }
            static float opacity = 1.0f;
            ImGui::SliderFloat(oxorany("UI Opacity"), &opacity, 0.1f, 1.0f);
            ImGui::GetStyle().Alpha = opacity;
            ImGui::Text(oxorany("Current FPS: %.1f"), ImGui::GetIO().Framerate);
            if (ImGui::Button(oxorany("Exit Cheat"))) {
                main_thread_flag = false;
            }
            if (ImGui::Button(oxorany("Unload Cheat"))) {
                exit(0);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (MinimapIcon) DrawMinimapESP(ImGui::GetForegroundDrawList());
    DrawMonster(ImGui::GetForegroundDrawList());
    g_window = ImGui::GetCurrentWindow();
    ImGui::End();

    // =================================================================
    // [ЭКРАНДАГЫ КАЛКЫП ЖҮРҮҮЧҮ КООЗ ТЕГЕРЕК БАСКЫЧ]
    // =================================================================
    if (gusion_combo_enabled) {
        ImGui::SetNextWindowPos(ImVec2(combo_icon_x, combo_icon_y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(combo_icon_size + 15.0f, combo_icon_size + 15.0f));
        
        ImGuiWindowFlags icon_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
        if (gusion_lock_position) {
            icon_window_flags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.00f, 0.75f, 1.00f, 1.00f)); 
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, combo_icon_size / 2.0f);            
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, combo_icon_size / 2.0f);             

        ImGui::Begin(oxorany("##FloatingComboIcon"), nullptr, icon_window_flags);
        
        if (ImGui::Button(oxorany("COMBO"), ImVec2(combo_icon_size, combo_icon_size))) {
            std::thread(ExecuteGusionUltraCombo).detach();
        }

        if (!gusion_lock_position) {
            ImVec2 current_icon_pos = ImGui::GetWindowPos();
            combo_icon_x = current_icon_pos.x;
            combo_icon_y = current_icon_pos.y;
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }
}

__attribute__((visibility("default"))) int main(int argc, char *argv[]) {
    pid = pidof(oxorany("com.mobile.legends:UnityKillsMe"));
    g_pid = pid;
    libbase = GetBase(oxorany("libcsharp.so"));
    printf("Lib: %p \n", (void*)libbase);
    screen_config();
    ::abs_ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::abs_ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) {
        return -1;
    }
    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, false);
    LoadConfiguration(); 
    ImGui::GetStyle().WindowRounding = 25.0f;
    while (main_thread_flag) {
        MonsterRetribution();
        CheckAndTriggerRetribution();
        drawBegin();
        Layout_tick_UI();
        drawEnd();
        usleep(1000);
    }
    shutdown();
    Touch_Close();
    return 0;
}
