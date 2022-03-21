#ifndef ROGUELIKE_H
#define ROGUELIKE_H

#include "libtcod.h"

namespace Roguelike {

	struct Creature;

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


	bool isCmd(std::string cmd);
}
#endif
