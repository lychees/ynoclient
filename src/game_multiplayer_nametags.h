#include <map>
#include <unordered_map>

#include "game_character.h"
#include "drawable.h"
#include "drawable_mgr.h"

/*
	================
	NAMETAG RENDERER
	================
*/

// one Drawable is responsible for drawing all name tags,
// so that it can control how to render when multiple nametags intersect.
class DrawableNameTags : public Drawable {
	struct Tag {
		BitmapRef renderGraphic;
		Game_Character* anchor;
		int x = 0;
		int y = 0;
	};
	struct TagStack {
		std::vector<Tag*> stack;
		float swayAnim = 0;
	};
	// Store nametags indexable by UID.
	// Also store list of nametags per tile, so they're drawn stacked on each other by iterating through occupied tiles.
	std::map<std::string, std::unique_ptr<Tag>> nameTags; // tags indexed by UID.
	std::unordered_map<unsigned long, TagStack> nameStacks; // list of tags per tile.

	// key is a hash based on tile coordinates, and value is a list of nametags on that tile.
	// perfect hash for coordinate pairs
	unsigned long coordHash(int x, int y); 


	void buildTagGraphic(Tag* tag, std::string name);
public:
	DrawableNameTags();

	void Draw(Bitmap& dst);

	void createNameTag(std::string uid, Game_Character* anchor);

	void deleteNameTag(std::string uid);

	void clearNameTags();

	void moveNameTag(std::string uid, int x, int y);

	void setTagName(std::string uid, std::string name);
};

extern std::unique_ptr<DrawableNameTags> nameTagRenderer; //global nametag renderer

/*
	================
	================
	================
*/
