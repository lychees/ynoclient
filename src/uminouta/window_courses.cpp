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

	item_max = 8;

	CreateContents();

	contents->Clear();

	for (int i = 0; i < item_max; ++i) {
		DrawItem(i);
	}

	SetIndex(index);
	// SetIndex(1);
}

const static std::string Courses_Title[16] = {
	"文学",
	"科学",
	"史学",
	"武学",
	"魔法",
	"兵法",
	"礼法",
	"历法",
};

const static std::string Courses_Help[16] = {
	"文学应该预见未来。",
	"科学的敌人，不比朋友少。",
	"以史为鉴，可知兴替。",
	"文講八法，武講八勢。",
	"用魔法打败魔法。",
	"知彼知己，百戰不殆",
	"人無禮不立，事無禮不成，國無禮不寧。",
	"脚踏实地，仰望星空。",
};

void Window_Courses::DrawItem(int id) {


	auto player = Roguelike::get_Player();
	int color = Font::ColorDefault;

	// contents->TextDraw(rect.x + rect.width - 24, rect.y, color, player.buffs[id]);

	std::string title = Courses_Title[id];

	// contents->TextDraw(rect.x, rect.y, color, title + "1");
	//contents->TextDraw(rect.x, rect.y, color, "123232123", Text::AlignRight);

	//Rect rect = GetItemRect(id);
	//contents->ClearRect(rect);

	contents->ClearRect(Rect(0, menu_item_height * i, contents->GetWidth() - 0, menu_item_height));
	contents->TextDraw(0, menu_item_height * i + menu_item_height / 8, color, title);
	contents->TextDraw(GetWidth() - 16, menu_item_height * i + menu_item_height / 8, color, "12321232123", Text::AlignRight);
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