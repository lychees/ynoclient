#ifndef EP_ROGUELIKE_H
#define EP_ROGUELIKE_H

#include "libtcod.hpp"

using std::vector;

namespace Roguelike {
	vector<vector<int> > Gen(int n, int m) {
		vector<int> z; z.resize(n);
		for (int i=0;i<m;++i) {
			z.push_back((std::rand() & 1) ? 1 : 0);
		}
		return z;
	}
} // namespace Roguelike


#endif
