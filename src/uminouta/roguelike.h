#ifndef ROGUELIKE_H
#define ROGUELIKE_H

#include "libtcod.h"

namespace Roguelike {
	std::vector<int>& get__A();
	std::vector<int>& get_A();
	std::vector<std::pair<int, int>>& get_empty_grids();
	void UpdateFOV();

	bool isInFOV(int x, int y);
	bool isExplored(int x, int y);
	void Gen(int c0, int c1);
}
#endif
