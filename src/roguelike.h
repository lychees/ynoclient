#ifndef EP_ROGUELIKE_H
#define EP_ROGUELIKE_H

#include "libtcod.hpp"

using std::vector;

namespace Roguelike {
	vector<vector<int>> Gen(int h, int w) {
		vector<vector<int>> z; z.resize(h);
		for (int i=0;i<h;++i) {
			for (int j=0;j<w;++j) {
				z[i].push_back((std::rand() & 1) ? 1 : 0);
			}
		}
		return z;
	}
} // namespace Roguelike


#endif
