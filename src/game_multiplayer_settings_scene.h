#include "scene.h"
#include "window.h"
#include "window_base.h"
#include "window_help.h"
#include "window_selectable.h"
#include "font.h"
#include "input.h"

namespace Game_Multiplayer
{
	class Window_Multiplayer : public Window_Selectable {
		public:
		Window_Multiplayer(int ix, int iy, int iwidth, int iheight);
		void Update() override;
		void Refresh();

		class SettingsItem {
			public:

			std::string name;
			std::string text_right;
			std::string help;
			void (*onAction) (SettingsItem& item, Input::InputButton action);

			Font::SystemColor color;
			Font::SystemColor color_right;

			SettingsItem(const std::string& name, const std::string& help, void (*onAction) (SettingsItem& item, Input::InputButton action) = nullptr, const std::string& text_right = "");
		};

		void Action(Input::InputButton action);

		private:
		void DrawItems();
		void DrawItem(int index);
		std::vector<SettingsItem> items;
	};

	class Scene_MultiplayerSettings : public Scene {
		public:

		Scene_MultiplayerSettings();
		void Start() override;
		void Update() override;

		private:
	
		std::unique_ptr<Window_Multiplayer> settings_window;
		std::unique_ptr<Window_Help> help_window;
	};
} // namespace Game_Multiplayer
