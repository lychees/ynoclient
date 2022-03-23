/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include <iomanip>
#include <sstream>
#include "window_quirks.h"
#include "roguelike.h"
#include "../game_actor.h"
#include "../game_actors.h"
#include "../game_party.h"
#include "../bitmap.h"
#include "../font.h"
#include "../player.h"
#include "../output.h"
#include <lcf/reader_util.h>
#include "../game_battle.h"


Window_Quirks::Window_Quirks(int ix, int iy, int iwidth, int iheight) :
	Window_Selectable(ix, iy, iwidth, iheight), actor_id(0), column_max(2) {

}

void Window_Quirks::SetActor(int actor_id) {
	this->actor_id = actor_id;
	Refresh();
}

void Window_Quirks::Refresh() {
	// data.clear();

	auto player = Roguelike::get_Player();

	/*const std::vector<int16_t>& skills = Main_Data::game_actors->GetActor(actor_id)->GetSkills();
	for (size_t i = 0; i < skills.size(); ++i) {
		if (CheckInclude(skills[i]))
			data.push_back(skills[i]);
	}

	if (data.size() == 0) {
		data.push_back(0);
	}*/

	item_max = player.buffs.size();

	CreateContents();

	contents->Clear();

	for (int i = 0; i < item_max; ++i) {
		DrawItem(i);
	}
}

void Window_Quirks::DrawItem(int id) {
	Rect rect = GetItemRect(id);
	contents->ClearRect(rect);

	auto player = Roguelike::get_Player;
	int color = Font::ColorDefault;

	contents->TextDraw(rect.x + rect.width - 24, rect.y, color, player.buffs[id]);

	/*fmt::format("{}{:3d}", lcf::rpg::Terms::TermOrDefault(lcf::Data::terms.easyrpg_skill_cost_separator, "-"), costs)*/ );
	// Skills are guaranteed to be valid
	//DrawSkillName(, rect.x, rect.y, enabled);
	//}
}

void Window_Quirks::UpdateHelp() {
	//help_window->SetText(GetSkill() == nullptr ? "" : ToString(GetSkill()->description));
	help_window->SetText("233");
}