#ifndef EP_ROGUELIKE_H
#define EP_ROGUELIKE_H

#include "libtcod.hpp"

using std::vector;

namespace Roguelike {

	vector<vector<int>> A;
	int h, w;
	const int ROOM_MAX_SIZE = 12;
	const int ROOM_MIN_SIZE = 6;

	void dig(int x1, int y1, int x2, int y2) {
		if (x1 > x2) swap(x1, x2);
		if (y1 > y2) swap(y1, y2);
		for (int i=x1; i<=x2; ++i) {
			for (int j=y1; j<=y2; ++j) {
				A[i][j] = 1;
			}
		}
	}

	class BspListener : public ITCODBspCallback {
	private :
		int lastx, lasty, roomNum;
	public :
		BspListener() :  {
			roomNum = 0;
		}
		bool visitNode(TCODBsp *node, void *userData) {
			if ( node->isLeaf() ) {
				int x,y,w,h;
				TCODRandom *rng=TCODRandom::getInstance();
				w=rng->getInt(ROOM_MIN_SIZE, node->w-2);
				h=rng->getInt(ROOM_MIN_SIZE, node->h-2);
				x=rng->getInt(node->x+1, node->x+node->w-w-1);
				y=rng->getInt(node->y+1, node->y+node->h-h-1);
				dig(x, y, x+w-1, y+h-1);
				if ( roomNum != 0 ) {
					dig(lastx,lasty,x+w/2,lasty);
					dig(x+w/2,lasty,x+w/2,y+h/2);
				}
				lastx=x+w/2;
				lasty=y+h/2;
				roomNum++;
			}
			return true;
		}
	};

	vector<vector<int>> Gen(int _h, int _w) {
		h = _h; w = _w;
		A.clear(); A.resize(h);
		for (int i=0;i<h;++i) {
			for (int j=0;j<w;++j) {
				A[i].push_back(0);
			}
		}
		TCODBsp bsp(0,0,w,h);
		bsp.splitRecursive(NULL,8,ROOM_MAX_SIZE,ROOM_MAX_SIZE,1.5f,1.5f);
		BspListener listener(*this);
		bsp.traverseInvertedLevelOrder(&listener,NULL);

		return A;
	}
} // namespace Roguelike


#endif
