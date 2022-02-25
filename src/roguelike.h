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
		if (x1 > x2) std::swap(x1, x2);
		if (y1 > y2) std::swap(y1, y2);
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
		BspListener() {
			roomNum = 0;
		}
		bool visitNode(TCODBsp *node, void *userData) {
			if ( node->isLeaf() ) {
				int x,y,h,w;
				TCODRandom *rng=TCODRandom::getInstance();
				h=rng->getInt(ROOM_MIN_SIZE, node->h-2);
				w=rng->getInt(ROOM_MIN_SIZE, node->w-2);
				x=rng->getInt(node->x+1, node->x+node->h-h-1);
				y=rng->getInt(node->y+1, node->y+node->w-w-1);

				dig(x, y, x+h-1, y+w-1);

				int xx = x+h/2;
				int yy = y+w/2;
				if ( roomNum != 0 ) {
					dig(lastx,lasty,xx,lasty);
					dig(xx,lasty,xx,yy);
				}
				lastx=xx;
				lasty=yy;
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
		TCODBsp bsp(0,0,h,w);
		bsp.splitRecursive(NULL,8,ROOM_MAX_SIZE,ROOM_MAX_SIZE,1.5f,1.5f);
		BspListener listener;
		bsp.traverseInvertedLevelOrder(&listener,NULL);

		return A;
	}
} // namespace Roguelike


#endif
