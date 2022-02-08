#include "game_multiplayer_settings_scene.h"
#include "game_multiplayer_connection.h"
#include "game_multiplayer_other_player.h"
#include "game_multiplayer_my_data.h"
#include "input.h"
#include "game_system.h"
#include "bitmap.h"
#include "output.h"
#include "font.h"

namespace Game_Multiplayer {

	void Reconnect(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		SetConnStatusWindowText("Disconnected");
		ConnectionData::connected = false;
		emscripten_websocket_deinitialize();
	}

	void SwitchSFXSync(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		MyData::sfxsync = !MyData::sfxsync;
		item.text_right = MyData::sfxsync ? "[ON]" : "[OFF]";
		item.color_right = MyData::sfxsync ? Font::SystemColor::ColorDefault : Font::SystemColor::ColorDisabled;
	}

	void SwitchNPCSync(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		MyData::syncnpc = !MyData::syncnpc;
		item.text_right = MyData::syncnpc ? "[ON]" : "[OFF]";
		item.color_right = MyData::syncnpc ? Font::SystemColor::ColorDefault : Font::SystemColor::ColorDisabled;
	}

	void SwitchNametags(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		MyData::rendernametags = !MyData::rendernametags;
		item.text_right = MyData::rendernametags ? "[ON]" : "[OFF]";
		item.color_right = MyData::rendernametags ? Font::SystemColor::ColorDefault : Font::SystemColor::ColorDisabled;
	}

	void ChangeSFXVolume(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		if(action == Input::InputButton::LEFT) {
			if(MyData::playersVolume > 0)
				MyData::playersVolume -= 2;
		} else if (action == Input::InputButton::RIGHT) {
			if(MyData::playersVolume < 100)
				MyData::playersVolume += 2;
		}
		item.text_right = std::to_string(MyData::playersVolume);
	}
	
	void ChangeSFXFalloff(Window_Multiplayer::SettingsItem& item, Input::InputButton action) {
		if(action == Input::InputButton::LEFT) {
			if(MyData::sfxfalloff > 4)
				MyData::sfxfalloff--;
		} else if (action == Input::InputButton::RIGHT) {
			if(MyData::sfxfalloff < 32)
				MyData::sfxfalloff++;
		}
		item.text_right = std::to_string(MyData::sfxfalloff);
	}

	Window_Multiplayer::Window_Multiplayer(int ix, int iy, int iwidth, int iheight) : 
	Window_Selectable(ix, iy, iwidth, iheight) {
		this->column_max = 1;
		this->index = 0;

		this->UpdateHelpFn = [this](Window_Help& win, int index) {
			if (index >= 0 && index < static_cast<int>(this->items.size())) {
				win.SetText(items[index].help);
			} else {
				win.SetText("");
			}
		};
		items.push_back(SettingsItem("SFX sync", "Lets you hear sound effects that other players produce", &SwitchSFXSync, MyData::sfxsync ? "[ON]" : "[OFF]"));
		items.push_back(SettingsItem("SFX falloff", "Maximum distance at which you can hear players", &ChangeSFXFalloff, std::to_string(MyData::sfxfalloff)));
		items.push_back(SettingsItem("Players SFX volume", "Volume of the sound effects that other players produce", &ChangeSFXVolume, std::to_string(MyData::playersVolume)));
		items.push_back(SettingsItem("NPC synchironisation", "Lets you and other players see NPC at same positions", &SwitchNPCSync, MyData::syncnpc ? "[ON]" : "[OFF]"));
		items.push_back(SettingsItem("Nametags", "Lets you see usernames of the players in game", &SwitchNametags, MyData::rendernametags ? "[ON]" : "[OFF]"));
		items.push_back(SettingsItem("Reconnect", "Disconnect, clear players & connect again", &Reconnect));
		this->item_max = items.size();
		CreateContents();
		SetEndlessScrolling(false);
	}

	void Window_Multiplayer::Update() {
		Window_Selectable::Update();
	}

	void Window_Multiplayer::Refresh() {
		DrawItems();
	}

	void Window_Multiplayer::DrawItems() {
		for(int i = 0; i < items.size(); i++) {
			DrawItem(i);
		}
	}

	void Window_Multiplayer::DrawItem(int i) {
		contents->ClearRect(Rect(0, menu_item_height * i, contents->GetWidth() - 0, menu_item_height));
		contents->TextDraw(0, menu_item_height * i + menu_item_height / 8, items[i].color, items[i].name);
		contents->TextDraw(GetWidth() - 16, menu_item_height * i + menu_item_height / 8, items[i].color_right, items[i].text_right, Text::AlignRight);
	}

	void Window_Multiplayer::Action(Input::InputButton action) {
		if(items[index].onAction) {
			items[index].onAction(items[index], action);
		}
	}

	Window_Multiplayer::SettingsItem::SettingsItem(const std::string& name, const std::string& help, void (*onAction) (SettingsItem& item, Input::InputButton action), const std::string& text_right) :
	name(name),
	text_right(text_right),
	help(help), 
	onAction(onAction), 
	color(Font::SystemColor::ColorDefault),
	color_right(Font::SystemColor::ColorDefault)
	{}

	Scene_MultiplayerSettings::Scene_MultiplayerSettings() {
		Scene::type = Scene::Multiplayer;
	}

	void Scene_MultiplayerSettings::Start() {
		settings_window.reset(new Window_Multiplayer(0, 32, SCREEN_TARGET_WIDTH, SCREEN_TARGET_HEIGHT - 32));
		help_window.reset(new Window_Help(0, 0, SCREEN_TARGET_WIDTH, 32));
		settings_window->SetHelpWindow(help_window.get());
		settings_window->Refresh();
	}
	void Scene_MultiplayerSettings::Update() {
		settings_window->Update();
		help_window->Update();

		//exit settings event
		if (Input::IsTriggered(Input::CANCEL)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cancel));
			Scene::Pop();
		} else if (Input::IsTriggered(Input::DECISION)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Decision));
			settings_window->Action(Input::DECISION);
			settings_window->Refresh();
		} else if (Input::IsTriggered(Input::LEFT)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cursor));
			settings_window->Action(Input::LEFT);
			settings_window->Refresh();
		} else if (Input::IsTriggered(Input::RIGHT)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Main_Data::game_system->SFX_Cursor));
			settings_window->Action(Input::RIGHT);
			settings_window->Refresh();
		}
	}
}