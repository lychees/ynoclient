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

#include "player.h"
#include <cstdlib>

// This is needed on Windows
#ifdef USE_SDL
#  include <SDL.h>
#endif

//#include "Engine.hpp"
//Engine engine;

#include "libtcod.h"

extern "C" int main(int argc, char* argv[]) {
	Player::Init(argc, argv);
	// Player::Run();

    /*while ( !TCODConsole::isWindowClosed() ) {
    	engine.update();
    	engine.render();
		TCODConsole::flush();
    }*/

    int playerx=40,playery=25;
    TCODConsole::initRoot(80,50,"libtcod C++ tutorial",false);
    while ( !TCODConsole::isWindowClosed() ) {
        TCOD_key_t key;
        TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS,&key,NULL);
        switch(key.vk) {
            case TCODK_UP : playery--; break;
            case TCODK_DOWN : playery++; break;
            case TCODK_LEFT : playerx--; break;
            case TCODK_RIGHT : playerx++; break;
            default:break;
        }
        TCODConsole::root->clear();
        TCODConsole::root->putChar(playerx,playery,'@');
        TCODConsole::flush();
    }
    return 0;

	return EXIT_SUCCESS;
}
