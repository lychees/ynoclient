#include "roguelike.h"
#include "../game_map.h"
#include "../main_data.h"
#include "../output.h"

namespace Roguelike {

	TCODMap *tcod_map;

	static const int ROOM_MAX_SIZE = 24;
	static const int ROOM_MIN_SIZE = 12;
	static const int dx[4] = {1,0,-1,0};
	static const int dy[4] = {0,1,0,-1};
	std::vector<int> A, _A; int w, h, c0, c1;
	int lu_x, lu_y, ld_x, ld_y;
	int ru_x, ru_y, rd_x, rd_y;
	std::vector<std::pair<int, int>> empty_grids;
	std::vector<std::vector<bool>> explored;
	bool fov_switch;

	bool isFOVon() {
		return fov_switch;
	}
	void turnon_FOV() {
		fov_switch = true;
	}
	void turnoff_FOV() {
		fov_switch = false;
	}

	void teleport_to(std::string who, int xx, int yy) {

		Output::Debug("Teleport To {] {}", xx, yy);

		if (who == "player") {
			auto tt = TeleportTarget::eForegroundTeleport;
			Main_Data::game_player->ReserveTeleport(Game_Map::GetMapId(), xx, yy, -1, tt);
		} else {
			for (auto& ev : Game_Map::GetEvents()) {
				/*if (ev.GetName() == who) {
					ev.SetX(xx); ev.SetY(yy);
				}*/
			}
		}
	}

	void teleport_to_lu(std::string who) {
		teleport_to(who, lu_x, lu_y);
	}
	void teleport_to_ld(std::string who) {
		teleport_to(who, ld_x, ld_y);
	}
	void teleport_to_ru(std::string who) {
		teleport_to(who, ru_x, ru_y);
	}
	void teleport_to_rd(std::string who) {
		teleport_to(who, rd_x, rd_y);
	}

	void teleport_to(std::string map_event_name) {

		/*for (const auto& ev : Game_Map::GetEvents) {
			if (ev.GetName() == "map_event_name") {

				return;
			}
		}*/

		/*for (int i=h-1;i>=0;--i) {
			for (int j=0;j<w;++j) {
				if (_A[i*w+j]) {
					auto tt = TeleportTarget::eForegroundTeleport;
					Main_Data::game_player->ReserveTeleport(GetMapId(), j, i, -1, tt);
					break;
				}
			}
		}*/
	}

	std::vector<std::pair<int, int>>& get_empty_grids() {
		return empty_grids;
	}

	std::vector<int>& get_A() {
		return A;
	}

	std::vector<int>& get__A() {
		return _A;
	}

	void dig(int x1, int y1, int x2, int y2) {
		if ( x2 < x1 ) std::swap(x1, x2);
		if ( y2 < y1 ) std::swap(y1, y2);
		for (int x=x1; x <= x2; ++x) {
			for (int y=y1; y <= y2; ++y) {
				A[x+y*w] = 1;
			}
		}
	}

	class BspListener : public ITCODBspCallback {
	private :
		int rr; // room number
		int xx, yy; // center of the last room
	public :
		BspListener() : rr(0) {}
		bool visitNode(TCODBsp *node, void *userData) {
			if (node->isLeaf()) {
				int x,y,w,h;
				// dig a room
				TCODRandom *rng=TCODRandom::getInstance();
				w=rng->getInt(ROOM_MIN_SIZE, node->w-2);
				h=rng->getInt(ROOM_MIN_SIZE, node->h-2);
				x=rng->getInt(node->x+1, node->x+node->w-w-1);
				y=rng->getInt(node->y+1, node->y+node->h-h-1);
				dig(x, y, x+w-1, y+h-1);

				for (int i=0;i<w;++i) {
					for (int j=0;j<h;++j) {
						empty_grids.push_back({y+j,x+i});
					}
				}

				x += w/2; y += h/2;
				if (rr) {
					// dig a corridor from last room
					dig(xx,yy,x,yy);
					dig(x,yy,x,y);
				}
				xx = x; yy = y; ++rr;
			}
			return true;
		}
	};

	bool inBound(int x, int y) {
		return 0 <= x && x < h && 0 <= y && y < w;
	}

	bool check(int x, int y) {
		return inBound(x, y) && !_A[x*w + y];
	}

	int autotile_offset(int x, int y) {
		int a = 0;
		bool lf = !check(x, y-1);
		bool up = !check(x-1, y);
		bool rt = !check(x, y+1);
		bool dn = !check(x+1, y);

		bool lu = !check(x-1, y-1);
		bool ru = !check(x-1, y+1);
		bool rd = !check(x+1, y+1);
		bool ld = !check(x+1, y-1);

		int cnt = int(lf) + int(up) + int(rt) + int(dn);

		if (cnt == 0) {
			if (lu) a += 1 << 0;
			if (ru) a += 1 << 1;
			if (rd) a += 1 << 2;
			if (ld) a += 1 << 3;
		} else if (cnt == 1) {
			if (lf) {
				a += (1 << 4) + 0;
				if (ru) a += 1;
				if (rd) a += 2;
			} else if (up) {
				a += (1 << 4) + 4;
				if (rd) a += 1;
				if (ld) a += 2;
			} else if (rt) {
				a += (1 << 4) + 8;
				if (ld) a += 1;
				if (lu) a += 2;
			} else { // dn
				a += (1 << 4) + 12;
				if (lu) a += 1;
				if (ru) a += 2;
			}
		} else if (cnt == 2) {

			a += (1 << 4) + 16;

			if (lf && rt) {
				a += 0;
			} else if (up && dn) {
				a += 1;
			} else if (up) {
				if (lf) {
					a += 2;
					if (rd) a += 1;
				} else { // rt
					a += 4;
					if (ld) a += 1;
				}
			} else { // dn
				if (rt) {
					a += 6;
					if (lu) a += 1;
				} else { // lt
					a += 8;
					if (ru) a += 1;
				}
			}
		} else if (cnt == 3) {

			a += (1 << 4) + 26;

			if (!dn) {
				a += 0;
			} else if (!rt) {
				a += 1;
			} else if (!up) {
				a += 2;
			} else { // up
				a += 3;
			}
		} else { // cnt == 4
			a += (1 << 4) + 30;
		}
		return a;
	}

	void Automatize() {

		_A = A;

		for (int i=0;i<h;++i) {
			for (int j=0;j<w;++j) {
				int &a = A[i*w+j]; a = _A[i*w+j] ? c1 : c0;
				if (4000 <= a && a <= 4550) a += autotile_offset(i,j);
			}
		}
	}

	void AddWall() {
		for (int i=0;i<h;++i) {
			for (int j=0;j<w;++j) {
			}
		}
	}

	void Gen(int _c0, int _c1) {
		c0 = _c0; c1 = _c1;
		empty_grids.clear();
		h = Game_Map::GetHeight(); w = Game_Map::GetWidth(); A.clear(); A.resize(w*h);
		TCODBsp bsp(0,0,w,h);
		bsp.splitRecursive(NULL,12,ROOM_MAX_SIZE,ROOM_MAX_SIZE,1.5f,1.5f);
    	BspListener listener;
    	bsp.traverseInvertedLevelOrder(&listener,NULL);

		delete tcod_map;
		tcod_map = new TCODMap(h, w);

		for (int i=0;i<h;++i) {
			for (int j=0;j<w;++j) {
				if (A[i*w+j]) {
					tcod_map->setProperties(i,j,true,true);
				} else {
					tcod_map->setProperties(i,j,false,false);
				}
			}
		}


		lu_x = -1; lu_y = -1;
		ld_x = -1; ld_y = -1;
		ru_x = -1; ru_y = -1;
		rd_x = -1; rd_y = -1;

		for (int i=h-1;i>=0;--i) {
			for (int j=0;j<w;++j) {
				if (A[i*w+j]) {
					if (lu_x == -1) {
						lu_x = j; lu_y = i;
					}
					if (ru_x == -1 || ru_x == j) {
						ru_x = j; ru_y = i;
					}
					if (ld_y == -1 || j > ld_y) {
						ld_x = j; ld_y = i;
					}
					rd_x = j; rd_y = i;
					break;
				}
			}
		}

		Automatize();
		// AddWall();

		explored.clear();
		explored.resize(h);
		for (int i=0;i<h;++i) {
			explored[i].resize(w);
		}
	}

	void UpdateFOV() {
		int my_y = Main_Data::game_player->GetX();
		int my_x = Main_Data::game_player->GetY();
		tcod_map->computeFov(my_x,my_y,20);
	}

	bool isInFOV(int x, int y) {
		if (tcod_map == nullptr) return true;
		bool z = tcod_map->isInFov(y,x);
		if (z) explored[y][x] = true;
		return z;
	}

	bool isExplored(int x, int y) {
		return explored[y][x];
	}

	bool isCmd(std::string msg) {
		std::string cmd;

		cmd = ".turnon_FOV";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			turnon_FOV();
			return true;
		}

		cmd = ".turnoff_FOV";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			turnoff_FOV();
			return true;
		}

		cmd = ".gen";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			std::istringstream iss(msg);
			std::string _; int c0, c1; iss >> _ >> c0 >> c1;
			Game_Map::Gen(c0, c1);
			return true;
		}

		cmd = ".roll";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			Game_Map::Roll();
			return true;
		}

		cmd = ".teleport_to_lu";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			std::istringstream iss(msg);
			std::string _, who; iss >> _ >> who;
			teleport_to_lu(who);
			return true;
		}

		cmd = ".teleport_to_ru";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			std::istringstream iss(msg);
			std::string _, who; iss >> _ >> who;
			teleport_to_ru(who);
			return true;
		}

		cmd = ".teleport_to_ld";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			std::istringstream iss(msg);
			std::string _, who; iss >> _ >> who;
			teleport_to_ld(who);
			return true;
		}

		cmd = ".teleport_to_rd";
		if (std::equal(cmd.begin(), cmd.end(), msg.begin())) {
			std::istringstream iss(msg);
			std::string _, who; iss >> _ >> who;
			teleport_to_rd(who);
			return true;
		}

		return false;
	}
};
