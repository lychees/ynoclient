#ifndef ROGUELIKE_H
#define ROGUELIKE_H

#include "libtcod.h"

namespace Roguelike {

	struct Creature {
		int str, dex, con;
		int inT, wis, cha;
		int evil, chaos;
		std::vector<std::string> buffs;
		std::string race;
		std::string alignments() {
			return "守序善良";
		}
		void set_race(std::string _race) {
			race = _race;
			str = dex = con = inT = wis = cha = 12;
			if (race == "Human") {
				// ..
			} else if (race == "Elf") {
				str -= 1; dex += 1; con -= 1;
				inT += 1; wis += 0; cha += 0;
			} else if (race == "Half Orc") {
				str += 2; dex -= 1; con += 2;
				inT -= 1; wis -= 1; cha -= 1;
			}
		}

		void add_buff(std::string buff) {
			buffs.push_back(buff);
			std::string cmd;
			cmd = "体弱多病";
			if (std::equal(cmd.begin(), cmd.end(), buff.begin())) {
				str -= 1; con -= 1;
				return;
			}
			cmd = "天资聪颖";
			if (std::equal(cmd.begin(), cmd.end(), buff.begin())) {
				inT += 1;
				return;
			}
			cmd = "身手敏捷";
			if (std::equal(cmd.begin(), cmd.end(), buff.begin())) {
				dex += 1;
				return;
			}
		}
	};

	Creature& get_Player();
	std::vector<int>& get__A();
	std::vector<int>& get_A();
	std::vector<std::pair<int, int>>& get_empty_grids();
	int get_c0();
	int get_c1();
	void UpdateFOV();

	bool isFOVon();
	void turnon_FOV();
	void turnoff_FOV();

	void teleport_to_lu(std::string who);
	void teleport_to_ld(std::string who);
	void teleport_to_ru(std::string who);
	void teleport_to_rd(std::string who);
	// void teleport_to(std::string map_event_name);

	bool isInFOV(int x, int y);
	bool isExplored(int x, int y);
	void Gen(int c0, int c1);

	void init();
	bool isCmd(std::string cmd);
}
#endif
