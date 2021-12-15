#include <map>
#include <memory>
#include <queue>
#include <set>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#include "game_multiplayer.h"
#include "chat_multiplayer.h"
#include "output.h"
#include "game_player.h"
#include "sprite_character.h"
#include "window_base.h"
#include "drawable_mgr.h"
#include "scene.h"
#include "bitmap.h"
#include "font.h"
#include "input.h"
#include "nxjson.h"
#include "cache.h"
#include "game_screen.h"
#include "game_variables.h"
#include "game_switches.h"
#include "game_map.h"
#include "player.h"

class MultiplayerText : Drawable {
	public:

		MultiplayerText() : Drawable(Priority_Frame) {
		DrawableMgr::Register(this);
	}

	//sets character that text will be drawn on top
	void SetAnchorCharacter(std::shared_ptr<Game_Character> anchor) {
		this->anchor = anchor;
	}

	void RemoveAnchorCharacter() {
		this->anchor.reset();
	}

	//sets text and draws in into bitmap
	void SetText(std::string text) {
		this->text = text;

		if(text == "")
			return;

		Rect textrect = Font::Default()->GetSize(text);
		Rect scaledrect = textrect;

		if(scaledrect.width > maxWidth) {
			scaledrect = Rect(0, 0, maxWidth, scaledrect.height * maxWidth / scaledrect.width);
		}

		textbitmap = Bitmap::Create(textrect.width + 1, textrect.height + 1, true);
		scaledbitmap = Bitmap::Create(scaledrect.width + 1, scaledrect.height + 1);
		Color color = Color(255, 255, 255, 255);
		Text::Draw(*textbitmap, 0, 0, *Font::Default(), color, text);
		scaledbitmap->StretchBlit(*textbitmap, textbitmap->GetRect(), Opacity::Opaque());
	}

	void SetMaxWidth(int width) {
		this->maxWidth = width;
	}

	void Draw(Bitmap& dst) override {
		if(text == "")
			return;

		Rect rect = scaledbitmap->GetRect();

		dst.Blit(anchor->GetScreenX() - rect.width / 2, anchor->GetScreenY() - rect.height - TILE_SIZE * 2, *scaledbitmap, rect, Opacity::Opaque());
	}

	private:
	std::shared_ptr<Game_Character> anchor;
	std::string text;
	BitmapRef textbitmap;
	BitmapRef scaledbitmap;
	int maxWidth;
	int ttl;
	bool ttluse;
};

struct MPPlayer {
	std::queue<std::pair<int,int>> mvq; //queue of move commands
	std::shared_ptr<Game_PlayerOther> ch; //character
	//this one is used to save player speed before setting it to max speed when move queue is too long
	int moveSpeed;
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<MultiplayerText> nickname;
};

std::string multiplayer__my_name = "";

namespace {

	namespace PacketTypes {
		const uint16_t movement = 1;
		const uint16_t sprite = 2;
		const uint16_t sound = 3;
		const uint16_t weather = 4;
		const uint16_t name = 5;
		const uint16_t movementAnimationSpeed = 6;
		const uint16_t variable = 7;
		const uint16_t switchsync = 8;
	};

	namespace MultiplayerSettings {
		uint8_t playersVolume = 50;
		uint8_t mAnimSpeed = 2;
		//adding delay before setting weather since i dont fucking understand when it sets weather on entering another room
		uint8_t weatherSetDelay = 25;
		uint8_t weatherT = 0;
		int nextWeatherType = -1;
		int nextWeatherStrength = -1;
	
		int switchsync;
		std::set<int> syncedswitches = std::set<int>();

		std::string spritesheet = "";
		int spriteid = 0;
	}

	std::unique_ptr<Window_Base> conn_status_window;
	//const std::string server_url = "wss://dry-lowlands-62918.herokuapp.com/";
	std::string server_url = "";
	EMSCRIPTEN_WEBSOCKET_T socket;
	bool connected = false;
	std::string myuuid = "";
	int room_id = -1;
	std::map<std::string,MPPlayer> players;
	const std::string delimchar = "\uffff";

	#define SEND_BUFFER_SIZE 2048
	#define RECEIVE_BUFFER_SIZE 8192
	char sendBuffer[SEND_BUFFER_SIZE];
	char receiveBuffer[RECEIVE_BUFFER_SIZE];

	void TrySend(std::string msg) {
		if (!connected) return;
		unsigned short ready;
		emscripten_websocket_get_ready_state(socket, &ready);
		if (ready == 1) { //1 means OPEN
			emscripten_websocket_send_binary(socket, (void*)msg.c_str(), msg.length());
		}
	}

	void TrySend(const void* buffer, size_t size) {
		if (!connected) return;
		unsigned short ready;
		emscripten_websocket_get_ready_state(socket, &ready);
		if (ready == 1) { //1 means OPEN
			emscripten_websocket_send_binary(socket, (void*)buffer, size);
		}
	}

	void SetConnStatusWindowText(std::string s) {
		conn_status_window->GetContents()->Clear();
		conn_status_window->GetContents()->TextDraw(0, 0, Font::ColorDefault, s);
	}

	void SpawnOtherPlayer(std::string uid) {
		auto& player = Main_Data::game_player;
		auto& nplayer = players[uid].ch;
		nplayer = std::make_shared<Game_PlayerOther>();
		nplayer->SetX(player->GetX());
		nplayer->SetY(player->GetY());
		nplayer->SetSpriteGraphic(player->GetSpriteName(), player->GetSpriteIndex());
		nplayer->SetMoveSpeed(player->GetMoveSpeed());
		nplayer->SetMoveFrequency(player->GetMoveFrequency());
		nplayer->SetThrough(true);
		nplayer->SetLayer(player->GetLayer());
		//to-do: call operator[] once and save reference to use
		players[uid].nickname = std::make_unique<MultiplayerText>();
		players[uid].nickname->SetAnchorCharacter(nplayer);
		players[uid].nickname->SetMaxWidth(TILE_SIZE * 2.2);
		players[uid].nickname->SetText("Madosussy");

		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
			return;
		}
		auto old_list = &DrawableMgr::GetLocalList();
		DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
		players[uid].sprite = std::make_unique<Sprite_Character>(nplayer.get());
		DrawableMgr::SetLocalList(old_list);
	}
	void SendMainPlayerPos() {
		auto& player = Main_Data::game_player;
		uint16_t cmsg[3] = {
			PacketTypes::movement,
			(uint16_t)player->GetX(),
			(uint16_t)player->GetY()
		};
		TrySend((void*)cmsg, sizeof(uint16_t) * 3);
	}
	
	
	void SendMainPlayerMoveSpeed(int spd) {
		uint16_t m[2] = {PacketTypes::movementAnimationSpeed, (uint16_t)spd};
		TrySend(m, 4);
	}
	

	void SendMainPlayerSprite(std::string name, int index) {

		//sprite packet [uint16_t, uint16_t, null terminated string] (packet type, sprite id, sprite sheet)

		uint16_t msgprefix[2] = {
			PacketTypes::sprite,
			(uint16_t)index,
		};
		size_t s = sizeof(char) * (name.length()) + sizeof(uint16_t) * 2;
		memcpy(sendBuffer, msgprefix, sizeof(uint16_t) * 2);
		memcpy(sendBuffer + sizeof(uint16_t) * 2, name.c_str(), name.length());
		TrySend(sendBuffer, s);
	}

	void SendMainPlayerName() {
		if(multiplayer__my_name.length() == 0) 
			return;
		int s = multiplayer__my_name.length() + sizeof(uint16_t);
		memcpy(sendBuffer, &PacketTypes::name, sizeof(uint16_t));
		memcpy(sendBuffer + sizeof(uint16_t), multiplayer__my_name.c_str(), multiplayer__my_name.length());
		TrySend(sendBuffer, s);
	}

	//this assumes that the player is stopped
	void MovePlayerToPos(std::shared_ptr<Game_PlayerOther> &player, int x, int y) {
		if (!player->IsStopping()) {
			Output::Debug("MovePlayerToPos unexpected error: the player is busy being animated");
		}
		int dx = x - player->GetX();
		int dy = y - player->GetY();
		if (abs(dx) > 1 || abs(dy) > 1 || dx == 0 && dy == 0) {
			player->SetX(x);
			player->SetY(y);
			return;
		}
		int dir[3][3] = {{Game_Character::Direction::UpLeft, Game_Character::Direction::Up, Game_Character::Direction::UpRight},
						 {Game_Character::Direction::Left, 0, Game_Character::Direction::Right},
						 {Game_Character::Direction::DownLeft, Game_Character::Direction::Down, Game_Character::Direction::DownRight}};
		player->Move(dir[dy+1][dx+1]);
	}

	extern "C" {
	void SlashCommandSetSprite(const char* sheet, int id);
	}
	EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) {
		std::string msg = "Connected to room " + std::to_string(room_id);
		std::string source = "Client";
		EM_ASM({
			PrintChatInfo(UTF8ToString($0), UTF8ToString($1));
		}, msg.c_str(), source.c_str());

		EM_ASM({
			ConnectToLocalChat($0);
		}, room_id);

		EM_ASM({
			SetRoomID($0)
		}, room_id);
		SetConnStatusWindowText("Connected");
		//puts("onopen");
		connected = true;
		auto& player = Main_Data::game_player;
		TrySend(Player::emscripten_game_name + "game");
		uint16_t room_id16[] = {(uint16_t)room_id};
		TrySend((void*)room_id16, sizeof(uint16_t));
		SendMainPlayerPos();
		if(MultiplayerSettings::spritesheet != "")
			SlashCommandSetSprite(MultiplayerSettings::spritesheet.c_str(), MultiplayerSettings::spriteid);
		SendMainPlayerSprite(player->GetSpriteName(), player->GetSpriteIndex());
		SendMainPlayerName();
		SendMainPlayerMoveSpeed((int)(MultiplayerSettings::mAnimSpeed));
		return EM_TRUE;
	}
	EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) {
		SetConnStatusWindowText("Disconnected");
		//puts("onclose");
		connected = false;

		return EM_TRUE;
	}

	void ResolveObjectSyncPacket(const nx_json* json);

	EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData) {
		if(websocketEvent->isText) {
			
			strcpy(receiveBuffer, (char*)websocketEvent->data);
			
			const nx_json* json = nx_json_parse(receiveBuffer, NULL);
			if(json) {
				const nx_json* typeNode = nx_json_get(json, "type");
				
				if(typeNode->type == nx_json_type::NX_JSON_STRING) {
					/*if(strcmp(typeNode->text_value, "fyllSync") == 0) {
						const nx_json* syncarray = nx_json_get(json, "data");
						if(syncarray->type == nx_json_type::NX_JSON_ARRAY) {
							for(int i = 0; i < syncarray->children.length; i++) {
								ResolveObjectSyncPacket(nx_json_item(syncarray, i));
							}
						}
					}*/

					if(strcmp(typeNode->text_value, "objectSync") == 0) {
						ResolveObjectSyncPacket(json);
					}
					else
					if(strcmp(typeNode->text_value, "disconnect") == 0) {
						auto scene_map = Scene::Find(Scene::SceneType::Map);
						const nx_json* uid = nx_json_get(json, "uuid");
						auto old_list = &DrawableMgr::GetLocalList();
						DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
						players[uid->text_value].nickname->RemoveAnchorCharacter();
						players.erase(uid->text_value);
						DrawableMgr::SetLocalList(old_list);
					}
				}
				
			}
			
			nx_json_free(json);
			
		}
		return EM_TRUE;
	}


	void ResolveObjectSyncPacket(const nx_json* json) {
		if(json->type == nx_json_type::NX_JSON_OBJECT) {
			const nx_json* pos = nx_json_get(json, "pos");
			const nx_json* path = nx_json_get(json, "path");;
			const nx_json* sprite = nx_json_get(json, "sprite");
			const nx_json* sound = nx_json_get(json, "sound");
			const nx_json* uid = nx_json_get(json, "uid");
			const nx_json* name = nx_json_get(json, "name");
			const nx_json* weather = nx_json_get(json, "weather");
			const nx_json* mAnimSpd = nx_json_get(json, "movementAnimationSpeed");
			const nx_json* variable = nx_json_get(json, "varialbe");
			const nx_json* switchsync = nx_json_get(json, "switchsync");
			
			if(uid->type == nx_json_type::NX_JSON_STRING) {
				std::string uids = std::string(uid->text_value);
				auto player = players.find(uids);
				if(player == players.cend()) {
					SpawnOtherPlayer(std::string(uid->text_value));
				}
					

				if(pos->type == nx_json_type::NX_JSON_OBJECT) {
					players[uids].mvq.push(std::make_pair(nx_json_get(pos, "x")->num.u_value, nx_json_get(pos, "y")->num.u_value));
				}
				else if(path->type == nx_json_type::NX_JSON_ARRAY) {
					for(int i = 0; i < path->children.length; i++) {
						pos = nx_json_item(path, i);
						if(pos->type == nx_json_type::NX_JSON_OBJECT) {
							players[uids].mvq.push(std::make_pair(nx_json_get(pos, "x")->num.u_value, nx_json_get(pos, "y")->num.u_value));
						}
					}
				}

				if(sprite->type == nx_json_type::NX_JSON_OBJECT) {
					const nx_json* sheet = nx_json_get(sprite, "sheet");
					const nx_json* id = nx_json_get(sprite, "id");
					players[uids].ch->SetSpriteGraphic(std::string(sheet->text_value), id->num.u_value);
				}

				if(sound->type == nx_json_type::NX_JSON_OBJECT) {
					const nx_json* volume = nx_json_get(sound, "volume");
					const nx_json* tempo = nx_json_get(sound, "tempo");
					const nx_json* balance = nx_json_get(sound, "balance");
					const nx_json* name = nx_json_get(sound, "name");

					lcf::rpg::Sound soundStruct;
					auto& p = players[uids];
					int dx = p.ch->GetX() - Main_Data::game_player->GetX();
					int dy = p.ch->GetY() - Main_Data::game_player->GetY();
					int distance = std::sqrt(dx * dx + dy * dy);
					soundStruct.volume = std::max(0, 
					(int)
					((100.0f - ((float)distance) * 10.0f) * (float(MultiplayerSettings::playersVolume) / 100.0f) * (float(volume->num.u_value) / 100.0f))
					);
					soundStruct.tempo = tempo->num.u_value;
					soundStruct.balance = balance->num.u_value;
					soundStruct.name = std::string(name->text_value);

					Main_Data::game_system->SePlay(soundStruct);
				}

				if(name->type == nx_json_type::NX_JSON_STRING) {
					players[uids].nickname->SetText(name->text_value);
				}

				if(weather->type == nx_json_type::NX_JSON_OBJECT) {
					const nx_json* type = nx_json_get(weather, "type");
					const nx_json* strength = nx_json_get(weather, "strength");
					Main_Data::game_screen.get()->SetWeatherEffect(type->num.u_value, strength->num.u_value);
				}

				if(mAnimSpd->type == nx_json_type::NX_JSON_INTEGER) {
					players[uids].moveSpeed = mAnimSpd->num.u_value;
				}

				if(variable->type == nx_json_type::NX_JSON_OBJECT && false) {
					const nx_json* id = nx_json_get(variable, "id");
					const nx_json* value = nx_json_get(variable, "value");
					Main_Data::game_variables->Set(id->num.u_value, value->num.s_value);
					Game_Map::SetNeedRefresh(true);
					
					std::string setvarstr = std::to_string(id->num.u_value) + " " + std::to_string(value->num.s_value);
					std::string varstr = "var";
					EM_ASM({
						PrintChatInfo(UTF8ToString($0), UTF8ToString($1));
					}, setvarstr.c_str(), varstr.c_str());
				}

				if(switchsync->type == nx_json_type::NX_JSON_OBJECT && MultiplayerSettings::switchsync) {
					const nx_json* id = nx_json_get(switchsync, "id");
					const nx_json* value = nx_json_get(switchsync, "value");
					if(MultiplayerSettings::syncedswitches.find(id->num.u_value) != MultiplayerSettings::syncedswitches.cend()) {
						Main_Data::game_switches->Set(id->num.u_value, value->num.s_value);
						Game_Map::SetNeedRefresh(true);
					}
					std::string setswtstr = std::to_string(id->num.u_value) + " " + std::to_string(value->num.s_value);
					EM_ASM({
						console.log("switch " + UTF8ToString($0));
					}, setswtstr.c_str());
				}
			}
		}
	}
}

//this will only be called from outside
extern "C" {

void gotMessage(const char* name, const char* trip, const char* msg, const char* src) {
	#if defined(INGAME_CHAT)
		Chat_Multiplayer::gotMessage(name, trip, msg, src);
	#endif
}

void gotChatInfo(const char* source, const char* text) {
	#if defined(INGAME_CHAT)
		Chat_Multiplayer::gotInfo(text);
	#endif
}

void SendChatMessage(const char* msg) {
	EM_ASM({
		SendMessageString(UTF8ToString($0));
	}, msg);
}

void ChangeName(const char* name) {
	multiplayer__my_name = name;
	SendMainPlayerName();
}

void SlashCommandSetSprite(const char* sheet, int id) {
	MultiplayerSettings::spritesheet = sheet;
	MultiplayerSettings::spriteid = id;
	if(MultiplayerSettings::spritesheet.length())
		Main_Data::game_player->SetSpriteGraphic(sheet, id);
}

void SetPlayersVolume(int volume) {
	MultiplayerSettings::playersVolume = volume;
}

void SetWSHost(const char* host) {
	server_url = host;
}

void SetSwitchSync(int val) {
	MultiplayerSettings::switchsync = val;
}

void SetSwitchSyncWhiteList(int id, int val) {
	if(val) {
		MultiplayerSettings::syncedswitches.emplace(id);
	} else {
		MultiplayerSettings::syncedswitches.erase(id);
	}
}

void LogSwitchSyncWhiteList() {
	std::string liststr = "";
	for(auto& swt : MultiplayerSettings::syncedswitches) {
		liststr += std::to_string(swt) + ",";
	}

	EM_ASM({console.log(UTF8ToString($0));}, liststr.c_str());
}
}
void Game_Multiplayer::Connect(int map_id) {
	room_id = map_id;
	Game_Multiplayer::Quit();
	//if the window doesn't exist (first map loaded) then create it
	//else, if the window is visible recreate it
	if (conn_status_window.get() == nullptr || conn_status_window->IsVisible()) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
		}
		else {
			auto old_list = &DrawableMgr::GetLocalList();
			DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
			conn_status_window = std::make_unique<Window_Base>(0, SCREEN_TARGET_HEIGHT-30, 100, 30);
			conn_status_window->SetContents(Bitmap::Create(100, 30));
			conn_status_window->SetZ(2106632960);
			DrawableMgr::SetLocalList(old_list);
		}
	}
	SetConnStatusWindowText("Disconnected");

	#if defined(INGAME_CHAT)
		//set up chat window if needed
		Chat_Multiplayer::tryCreateChatWindow();
	#endif

	std::string room_url = server_url + std::to_string(map_id);
	Output::Debug(room_url);
	EmscriptenWebSocketCreateAttributes ws_attrs = {
		server_url.c_str(),
		"binary",
		EM_TRUE
	};

	socket = emscripten_websocket_new(&ws_attrs);
	emscripten_websocket_set_onopen_callback(socket, NULL, onopen);
	//emscripten_websocket_set_onerror_callback(socket, NULL, onerror);
	emscripten_websocket_set_onclose_callback(socket, NULL, onclose);
	emscripten_websocket_set_onmessage_callback(socket, NULL, onmessage);
}

void Game_Multiplayer::Quit() {
	emscripten_websocket_deinitialize(); //kills every socket for this thread
	for(auto& p : players) {
		p.second.nickname->RemoveAnchorCharacter();
	}
	players.clear();
}

void Game_Multiplayer::MainPlayerMoved(int dir) {
	SendMainPlayerPos();
}

void Game_Multiplayer::MainPlayerChangedMoveSpeed(int spd) {
	MultiplayerSettings::mAnimSpeed = spd;
	SendMainPlayerMoveSpeed(spd);
}

void Game_Multiplayer::MainPlayerChangedSpriteGraphic(std::string name, int index) {
	SendMainPlayerSprite(name, index);
}

void Game_Multiplayer::SePlaySync(const lcf::rpg::Sound& sound) {
	if(sound.volume == 0)
		return;
	size_t s = sizeof(uint16_t) * 4 + sound.name.length();
	uint16_t vtd[4] = {PacketTypes::sound, (uint16_t)sound.volume, (uint16_t)sound.tempo, (uint16_t)sound.balance};
	memcpy(sendBuffer, vtd, sizeof(uint16_t) * 4);
	memcpy(sendBuffer + sizeof(uint16_t) * 4, sound.name.c_str(), sound.name.length());
	TrySend(sendBuffer, s);
}

void Game_Multiplayer::WeatherEffectSync(int type, int strength) {
	MultiplayerSettings::nextWeatherType = type;
	MultiplayerSettings::nextWeatherStrength = strength;
}

void Game_Multiplayer::VariableSync(int32_t id, int32_t val) {
	return;
	//uint16_t ptype = PacketTypes::variable;
	//memcpy(sendBuffer, &ptype, sizeof(uint16_t));
	//int32_t m[2] = {id, val};
	//memcpy(sendBuffer + sizeof(uint16_t), m, sizeof(int32_t) * 2);
	//TrySend(&sendBuffer, sizeof(int16_t) * 5);
}

void Game_Multiplayer::SwitchSync(int32_t id, int32_t val) {
	uint16_t ptype = PacketTypes::switchsync;
	memcpy(sendBuffer, &ptype, sizeof(uint16_t));
	int32_t m[2] = {id, val};
	memcpy(sendBuffer + sizeof(uint16_t), m, sizeof(int32_t) * 2);
	TrySend(&sendBuffer, sizeof(uint16_t) * 5);
}

void Game_Multiplayer::Update() {
	if(MultiplayerSettings::nextWeatherType != -1) {
		MultiplayerSettings::weatherT++;
		if(MultiplayerSettings::weatherT > MultiplayerSettings::weatherSetDelay) {
			MultiplayerSettings::weatherT = 0;
			uint16_t msg[3] = {PacketTypes::weather, (uint16_t)MultiplayerSettings::nextWeatherType, (uint16_t)MultiplayerSettings::nextWeatherStrength};
			TrySend(msg, sizeof(uint16_t) * 3);
			MultiplayerSettings::nextWeatherType = -1;
			MultiplayerSettings::nextWeatherStrength = -1;
		}
	}

	for (auto& p : players) {
		auto& q = p.second.mvq;
		if (!q.empty() && p.second.ch->IsStopping()) {
			MovePlayerToPos(p.second.ch, q.front().first, q.front().second);
			if(q.size() > 8) {
				p.second.ch->SetMoveSpeed(6);
				while(q.size() > 16)
						q.pop();
			} else {
				p.second.ch->SetMoveSpeed(p.second.moveSpeed);
			}
			q.pop();
		}
		p.second.ch->SetProcessed(false);
		p.second.ch->Update();
		p.second.sprite->Update();
	}
	if (Input::IsReleased(Input::InputButton::N3)) {
		conn_status_window->SetVisible(!conn_status_window->IsVisible());
	}
}

