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

#ifndef EP_WINDOW_COURSES_H
#define EP_WINDOW_COURSES_H

// Headers
#include <vector>
#include "../window_selectable.h"

/**
 * Window_Skill class.
 */
class Window_Courses : public Window_Selectable {

public:
	/**
	 * Constructor.
	 */
	Window_Courses(int ix, int iy, int iwidth, int iheight);

	/**
	 * Sets the actor whose courses are displayed.
	 *
	 * @param actor_id ID of the actor.
	 */
	void SetActor(int actor_id);

	/**
	 * Refreshes the skill list.
	 */
	virtual void Refresh();

	/**
	 * Draws a course together with the level.
	 *
	 * @param index index of skill to draw.
	 */
	void DrawItem(int index);

	/**
	 * Updates the help window.
	 */
	void UpdateHelp() override;

	int morning, afternoon, night, stage;

protected:

	int actor_id;
};

#endif
