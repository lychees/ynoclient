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
#include "window_courses.h"
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
#include "../output.h"


Window_Courses::Window_Courses(int ix, int iy, int iwidth, int iheight) :
	Window_Selectable(ix, iy, iwidth, iheight), actor_id(0) {
	column_max = 1;
}

void Window_Courses::SetActor(int actor_id) {
	this->actor_id = actor_id;
	Refresh();
}

void Window_Courses::Refresh() {
	// data.clear();

	auto player = Roguelike::get_Player();

	item_max = 9;

	CreateContents();

	contents->Clear();

	for (int i = 0; i < item_max; ++i) {
		DrawItem(i);
	}

	SetIndex(index);
	// SetIndex(1);
}

const static std::string Courses_Title[9] = {
	"诗",
	"书",
	"礼",
	"易",
	"琴",
	"弈",
	"舞",
	"画",
	"史",
	"理",
	"戎",
	"御",
	"骑",
	"射",
	"铳",
	"兵",
};

const static std::string Courses_Help[9] = {
	"诗歌被认为是文学最初的起源，其最初发生于尚未有文字的人类社会，以口语的形式流传，并与音乐、舞蹈结合。",
	"书法是书写文字的艺术。",
	"礼仪。",
	"魔法相关的知识。",
	"音乐理论与乐器相关的知识。",
	"游戏相关的知识。",
	"舞蹈相关的知识。",
	"历史相关的知识。",
	"物理相关的知识。",
	"冷兵器相关的知识。",
	"驾驶载具相关的知识。",
	"骑马相关的知识。",
	"射箭相关的知识。",
	"火器相关的知识。",
	"兵法。",
};

void Window_Courses::DrawItem(int id) {
	Rect rect = GetItemRect(id);
	contents->ClearRect(rect);

	auto player = Roguelike::get_Player();
	int color = Font::ColorDefault;

	// contents->TextDraw(rect.x + rect.width - 24, rect.y, color, player.buffs[id]);

	std::string title = Courses_Title[id];

	contents->TextDraw(rect.x, rect.y, color, title);
	// contents->TextDraw(rect.x, rect.y, color, "Lv " + std::to_string(player.quirks[id].second));

	/*fmt::format("{}{:3d}", lcf::rpg::Terms::TermOrDefault(lcf::Data::terms.easyrpg_skill_cost_separator, "-"), costs) );*/
	// Skills are guaranteed to be valid
	//DrawSkillName(, rect.x, rect.y, enabled);
	//}
}

void Window_Courses::UpdateHelp() {
	// Output::Debug("Update Help: 233");
	//help_window->SetText(GetSkill() == nullptr ? "" : ToString(GetSkill()->description));
	auto player = Roguelike::get_Player();
	std::string help;

	help_window->SetText(Courses_Help[index]);
}
