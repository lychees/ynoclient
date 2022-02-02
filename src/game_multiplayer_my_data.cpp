
#include "game_multiplayer_my_data.h"

namespace Game_Multiplayer {

std::string MyData::username;
uint8_t MyData::playersVolume = 50;
bool MyData::shouldsync = false;

int MyData::switchsync = 0;
std::set<int> MyData::syncedswitches = std::set<int>();
std::set<int> MyData::switchlogblacklist = std::set<int>(); //set of switch ids that souldn't be logged

//used for custom sprites
std::string MyData::spritesheet = "";
int MyData::spriteid = 0;

//adding delay before setting weather since i dont fucking understand when it sets weather on entering another room
uint8_t MyData::weatherSetDelay = 25;
uint8_t MyData::weatherT = 0;
int MyData::nextWeatherType = -1;
int MyData::nextWeatherStrength = -1;

}