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

void Window_Courses::DrawItem(int id) {
	Rect rect = GetItemRect(id);
	contents->ClearRect(rect);

	auto player = Roguelike::get_Player();
	int color = Font::ColorDefault;

	// contents->TextDraw(rect.x + rect.width - 24, rect.y, color, player.buffs[id]);

	std::string title;
	if (id == 0) {
		title = "文学";
	} else {
		title = "科学";
	}

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

	if (index == 0) {
		help = "文学是一种艺术形式，或被认为具有艺术或智力价值的任何单一作品，通常是由于以不同于普通用途的方式部署语言。";
	} if (index == 1) else {
		help = "科学是一种系统性的知识体系，它积累和组织并可检验有关于宇宙的解释和预测。";
	} else {
		help = "??";
	}

	help_window->SetText(help);
}
