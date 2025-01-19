// AOBScanner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include "Utils.h"
#include "MemoryUtility.h"
#include <format>
#include <map>
#include <sstream>
#include <cctype>
#include <fstream>
#include <sstream>

using namespace Utils;

static MemoryUtility* g_memoryUtility;
static HMODULE g_hModule = NULL;
const static std::string LOCATION_JSON_FILE = "Locations.json";

struct CHARACTER_INFO {
    std::string Name;
    std::string Class;
    std::string CurrentLocation;
    long PlayTime;
    std::string PlayTimeFmt;
    long DeathCount;
    long Runes;
    long HP;
    long MP;
    long SP;
    long Level;
    long Strength;
    long Dexterity;
    long Intelligence;
    long Faith;
    long Arcane;
    long Endurance;
    long Mind;
    long Vigor;
}CHARACTER;

struct GAME_INFO {
    uint16_t NetPlayerCount;
}GAME;

std::string ReadFile(const std::string& FILENAME) {
    std::ifstream FILE(FILENAME);

    if (!FILE.is_open()) {
        std::cerr << "Could no open the File: " << FILENAME << std::endl;

        return "";
    }

    std::cout << "File: " << FILENAME << " Successfully Opened!" << std::endl;

    std::cout << "Reading File." << std::endl;

    std::stringstream buffer;
    buffer << FILE.rdbuf();

    std::cout << "Done Reading the File." << std::endl;
    return buffer.str();
}

static std::string GET_CLASS_NAME(long CLASS_ID)
{
    switch (CLASS_ID)
    {
    case 0: return "Vagabond";
    case 1: return "Warrior";
    case 2: return "Hero";
    case 3: return "Bandit";
    case 4: return "Astrologer";
    case 5: return "Prophet";
    case 6: return "Confessor";
    case 7: return "Samurai";
    case 8: return "Prisoner";
    case 9: return "Wretch";
    }
}

static std::map<long, std::string> DeserializeJsonToMap(const std::string& json) {
    std::map<long, std::string> data;
    std::string key, value;
    bool inKey = false, inValue = false;
    std::stringstream ss(json);

    std::string clean;
    for (char characters : json)
        if (!std::isspace(characters) && characters != '{' && characters != '}')
            clean += characters;

    for (size_t i = 0; i < clean.length(); ++i) {
        char character = clean[i];

        if (character == '"') {
            if (!inKey && !inValue) {
                inKey = true;
            }
            else if (inKey && !inValue) {
                inKey = false;
            }
            else if (!inKey && inValue) {
                inValue = false;
                data[std::stol(key)] = value;
                key.clear();
                value.clear();
            }
            else {
                inValue = true;
            }
        }
        else {
            if (inKey) {
                key += character;
            }
            else if (inValue) {
                value += character;
            }
        }
    }

    return data;
}

static std::string GET_LOCATION_NAME(const std::map<long, std::string>& LOCATIONS, long LOCATION_ID) {
    auto temp = LOCATIONS.find(LOCATION_ID);

    if (temp != LOCATIONS.end()) {
        return temp->second;
    }
    else {
        return "Uknown Location";
    }
}

static std::map<std::string, long> ReadAllAttributes(MemoryUtility* memoryUtility) {
    std::map<std::string, long> attrs;
    attrs["Strength"] = memoryUtility->ReadStrengthAttr();
    attrs["Dexterity"] = memoryUtility->ReadDexterityAttr();
    attrs["Intelligence"] = memoryUtility->ReadIntelligenceAttr();
    attrs["Faith"] = memoryUtility->ReadFaithAttr();
    attrs["Arcane"] = memoryUtility->ReadArcaneAttr();
    attrs["Vigor"] = memoryUtility->ReadVigorAttr();
    attrs["Mind"] = memoryUtility->ReadMindAttr();
    attrs["Endurance"] = memoryUtility->ReadEnduranceAttr();

    CHARACTER.Strength = memoryUtility->ReadStrengthAttr();
    CHARACTER.Dexterity = memoryUtility->ReadDexterityAttr();
    CHARACTER.Intelligence = memoryUtility->ReadIntelligenceAttr();
    CHARACTER.Faith = memoryUtility->ReadFaithAttr();
    CHARACTER.Arcane = memoryUtility->ReadArcaneAttr();
    CHARACTER.Vigor = memoryUtility->ReadVigorAttr();
    CHARACTER.Mind = memoryUtility->ReadMindAttr();
    CHARACTER.Endurance = memoryUtility->ReadEnduranceAttr();

    return attrs;
}

static std::string GetHighestAttribute(const std::map<std::string, long>& attrs) {
    auto highest = std::max_element(attrs.begin(), attrs.end(),
        [](const std::pair<std::string, long>& a, const std::pair<std::string, long>& b) {
            return a.second < b.second;
        });

    return highest->first;
}

int main(int argc, const char* argv[])
{
    g_memoryUtility->initialize();

    std::string LOCATION_JSON = ReadFile(LOCATION_JSON_FILE);

    if (LOCATION_JSON.empty()) {
        return EXIT_FAILURE;
    }

    std::cout << "Mapping Json File" << std::endl;
    std::map<long, std::string> LOCATION_DATA = DeserializeJsonToMap(LOCATION_JSON);

    std::cout << "Done Mapping Json File!" << std::endl;

    while (true)
    {
        CHARACTER.Name = g_memoryUtility->ReadPlayerName(0);

        if (CHARACTER.Name != "")
        {
            GAME.NetPlayerCount = g_memoryUtility->CountNetPlayers();
            long LOCATION_ID = g_memoryUtility->ReadLastGraceLocationId();
            uint16_t CLASS_ID = g_memoryUtility->ReadPlayerClassId();

            CHARACTER.Level = g_memoryUtility->ReadLocalPlayerLevel();
            CHARACTER.CurrentLocation = GET_LOCATION_NAME(LOCATION_DATA, LOCATION_ID);
            CHARACTER.Class = GET_CLASS_NAME(CLASS_ID);
            

            auto ATTRIBUTES = ReadAllAttributes(g_memoryUtility);
            std::string HIGHEST_ATTRIBUTE = GetHighestAttribute(ATTRIBUTES);

            CHARACTER.PlayTime = g_memoryUtility->ReadPlayTime();
            double Hours = CHARACTER.PlayTime / (1000.0 * 60 * 60);
            CHARACTER.PlayTimeFmt = std::format("{:.2f}", Hours);
            
            CHARACTER.DeathCount = g_memoryUtility->ReadDeathCount();

            CHARACTER.Runes = g_memoryUtility->ReadRunesHeld();

            std::cout << "[X] Player Count: " << GAME.NetPlayerCount << std::endl
                << "[X] Character Name: " << CHARACTER.Name << std::endl
                << "[X] Level: " << CHARACTER.Level << std::endl
                << "[X] Class: " << CHARACTER.Class << std::endl
                << "[X] Build Type: " << HIGHEST_ATTRIBUTE << std::endl
                << "[X] Current Location: " << CHARACTER.CurrentLocation << std::endl
                << "[X] Play Time: " << CHARACTER.PlayTimeFmt << std::endl
                << "[X] Runes: " << CHARACTER.Runes << std::endl
                << "[X] Health: " << CHARACTER.HP << std::endl
                << "[X] Mana: " << CHARACTER.MP << std::endl
                << "[X] Stamina: " << CHARACTER.SP << std::endl
                << "[X] Death Count: " << CHARACTER.DeathCount << std::endl
                << "[X] Strength: " << CHARACTER.Strength << std::endl
                << "[X] Dexterity: " << CHARACTER.Dexterity << std::endl
                << "[X] Intelligence: " << CHARACTER.Intelligence << std::endl
                << "[X] Faith: " << CHARACTER.Faith << std::endl
                << "[X] Arcance: " << CHARACTER.Arcane << std::endl
                << "[X] Endurance: " << CHARACTER.Endurance << std::endl
                << "[X] Mind: " << CHARACTER.Mind << std::endl
                << "[X] Vigor: " << CHARACTER.Vigor << std::endl;

            system("cls");
        }
    }

    return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
