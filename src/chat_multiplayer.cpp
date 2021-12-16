#include "chat_multiplayer.h"
#include <memory>
#include <emscripten/emscripten.h>
#include <vector>
#include <utility>
#include <regex>
#include "window_base.h"
#include "scene.h"
#include "bitmap.h"
#include "output.h"
#include "drawable_mgr.h"
#include "font.h"
#include "cache.h"
#include "input.h"
#include "utils.h"

extern "C" void SendChatMessage(const char* msg);
extern std::string multiplayer__my_name;

class ChatBox : public Drawable {
	struct Message {
		enum Visibility {
			VIS_LOCAL	= 1,
			VIS_GLOBAL	= 2,
		};
		BitmapRef renderGraphic;
		std::string colorA;
		std::string colorB;
		std::string colorC;
		unsigned short visibilityFlags;
		bool dirty = false; // need to redraw? (for when UI skin changes)
	};

	//design parameters
	const unsigned int panelBleed = 16; // how much to stretch top, bottom and right edges of panel offscreen (so only left frame shows)
	const unsigned int panelFrame = 4; // width of panel's visual frame (on left side)
	const unsigned int typeHeight = 24; // height of type box
	const unsigned int typePaddingHorz = 8; // padding between type box edges and content (left)
	const unsigned int typePaddingVert = 6; // padding between type box edges and content (top)
	const unsigned int messageMargin = 3; // horizontal margin between panel edges and content
	const unsigned int namePromptMargin = 40; // left margin for type box to make space for "name:" prompt
	
	Window_Base backPanel; // background pane
	Window_Base scrollBox; // box used as rendered design for a scrollbar
	std::vector<Message> messages;
	unsigned int scrollPosition = 0;
	unsigned int scrollContentHeight = 0; // height of scrollable message log (including offscren) (only accounts for visible messages)
	unsigned short visibilityMask = Message::VIS_LOCAL|Message::VIS_GLOBAL; // bitmask denoting visible message types, according to Message::Visibility enum
	bool focused = false;
	BitmapRef typeText;
	std::vector<unsigned int> typeCharOffsets; // cumulative x offsets for each character in the type box. Used for rendering the caret
	BitmapRef typeCaret;
	unsigned int typeCaretIndex = 0;
	unsigned int typeScroll = 0; // horizontal scrolling of type box
	bool nameInputEnabled = true;
	BitmapRef namePrompt; // graphic used for the "name:" prompt

	bool messageVisible(Message& m, unsigned short visMask) {
		return (visMask & m.visibilityFlags) > 0;
	}

	// gets height of screen area that shows chat log.
	// Varies on whether or not the chatbox is focused (as the typebox appears)
	unsigned int getLogVisibleHeight() {
		return (focused) ?
			SCREEN_TARGET_HEIGHT-typeHeight
		:	SCREEN_TARGET_HEIGHT;
	}

	void updateScrollBar() {
		const unsigned int logVisibleHeight = getLogVisibleHeight();
		if(scrollContentHeight <= logVisibleHeight) {
			scrollBox.SetX(TOTAL_TARGET_WIDTH); // hide scrollbar if content isn't large enough for scroll
			return;
		}
		scrollBox.SetX(TOTAL_TARGET_WIDTH-panelFrame); // show scrollbar
		// position scrollbar
		const float ratio = logVisibleHeight/float(scrollContentHeight);
		const unsigned int barHeight = logVisibleHeight*ratio;
		const unsigned int barY = scrollPosition*ratio;
		scrollBox.SetHeight(barHeight);
		scrollBox.SetY(logVisibleHeight-barY-barHeight);
	}

	void updateTypeBox() {
		if(!focused) {
			backPanel.SetCursorRect(Rect(0, 0, 0, 0));
		} else {
			const unsigned int f = -8; // SetCursorRect for some reason already has a padding of 8px relative to the window, so we fix it
			const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
			backPanel.SetCursorRect(Rect(f+panelFrame+namePad, f+panelBleed+SCREEN_TARGET_HEIGHT-typeHeight, CHAT_TARGET_WIDTH-panelFrame-namePad, typeHeight));
		}
	}

	BitmapRef buildMessageGraphic(std::string colorA, std::string colorB, std::string colorC, unsigned int& graphicHeight) {
		// manual text wrapping
		const unsigned int maxWidth = CHAT_TARGET_WIDTH-panelFrame*2-messageMargin*2;
		std::vector<std::pair<std::string, unsigned int>> lines; // individual lines saved so far, along with their y offset
		unsigned int totalWidth = 0; // maximum width between all lines
		unsigned int totalHeight = 0; // accumulated height from all lines

		std::string currentLine = colorA+colorB+colorC; // current line being worked on. start with whole message.
		std::string nextLine = ""; // stores characters moved down to line below
		do {
			auto rect = Font::Tiny()->GetSize(currentLine);
			while(rect.width > maxWidth) {
				// as long as current line exceeds maximum width,
				// move one character from this line down to the next one
				nextLine = currentLine.back()+nextLine;
				currentLine.pop_back();
				// recalculate line width with that character having been moved down
				rect = Font::Tiny()->GetSize(currentLine);
			}
			// once line fits, check for line breaks
			unsigned int lineBreak = currentLine.find("\n");
			if(lineBreak != std::string::npos) {
				nextLine = currentLine.substr(lineBreak+1, std::string::npos)+nextLine;
				currentLine = currentLine.substr(0, lineBreak)+" ";
			}
			// save line
			lines.push_back(std::make_pair(currentLine, totalHeight));
			totalWidth = std::max<unsigned int>(totalWidth, rect.width);
			totalHeight += rect.height;
			// repeat this work on the exceeding portion moved down to the line below
			currentLine = nextLine;
			nextLine = "";
		} while(currentLine.size() > 0);

		// once all lines have been saved
		// render them into a bitmap
		BitmapRef text_img = Bitmap::Create(totalWidth+1, totalHeight+1, true);
		unsigned int colorACharacters = colorA.size(); // remaining A colored characters to draw
		unsigned int colorBCharacters = colorB.size(); // remaining B colored characters to draw
		int nLines = lines.size();
		for(int i = 0; i < nLines; i++) {
			auto& line = lines[i];
			// calculate number of colored characters to draw on this line
			unsigned int lineSize = line.first.size();
			unsigned int aChars = std::min<unsigned int>(colorACharacters, lineSize);
			unsigned int bChars = std::min<unsigned int>(colorBCharacters, lineSize-aChars);
			// decrement from total remaining colored characters
			colorACharacters -= aChars;
			colorBCharacters -= bChars;
			// draw the three colored text regions
			unsigned int bStart = Text::Draw(*text_img, 0, line.second, *Font::Tiny(), *Cache::SystemOrBlack(), 1, line.first.substr(0, aChars)).width;
			unsigned int cStart = Text::Draw(*text_img, bStart, line.second, *Font::Tiny(), *Cache::SystemOrBlack(), 2, line.first.substr(aChars, bChars)).width;
			Text::Draw(*text_img, bStart+cStart, line.second, *Font::Tiny(), *Cache::SystemOrBlack(), 0, line.first.substr(aChars+bChars, std::string::npos));
		}
		// return
		graphicHeight = totalHeight;
		return text_img;
	}

	void buildLabelGraphic(std::string label) {
		std::string prompt = label+":";
		auto nRect = Font::Default()->GetSize(prompt);
		namePrompt = Bitmap::Create(nRect.width+1, nRect.height+1, true);
		Text::Draw(*namePrompt, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 1, prompt);
	}

	// may change scroll content height, so assert scroll bounds and update scroll bar
	void changeVisibilityMask(unsigned short newVis) {
		//   Expands/collapses messages in-place, 
		// so you don't get lost if you've scrolled far up.
		//
		//   Finds the bottommost (before the change) message that is visible both before and after changing visibility flags, and
		// anchors it into place so it stays at the same visual location
		// before and after expanding/collapsing.

		// calculate new total content height for new visibility flags.
		unsigned int newContentHeight = 0;
		// save anchor position before and after
		int preAnchorY = -scrollPosition;
		int postAnchorY = -scrollPosition;
		bool anchored = false; // if true, anchor has been found so stop accumulating message heights
		//
		for(int i = messages.size()-1; i >= 0; i--) {
			bool preVis = messageVisible(messages[i], visibilityMask); // message is visible with previous visibility mask
			bool postVis = messageVisible(messages[i], newVis); // message is visible with new visibility mask
			unsigned int msgHeight = messages[i].renderGraphic->GetRect().height;
			// accumulate total content height for new visibility flags
			if(postVis) newContentHeight += msgHeight;

			if(!anchored) {
				if(preVis) preAnchorY += msgHeight;
				if(postVis) postAnchorY += msgHeight;
				bool validAnchor = preVis && postVis; // this message is an anchor candidate because it is visible both before and after
				if(validAnchor && preAnchorY > 0) {
					anchored = true; // this is the bottommost anchorable message
				}
			}
		}
		// updates scroll content height
		scrollContentHeight = newContentHeight;
		// adjusts scroll position so anchor stays in place
		int scrollDelta = postAnchorY-preAnchorY;
		int tempSignedScroll = scrollPosition+scrollDelta;
		// assert scroll bounds
		const unsigned int logVisibleHeight = getLogVisibleHeight();
		const unsigned int maxScroll = std::max<int>(0, scrollContentHeight-logVisibleHeight); // maximum value for scrollPosition (how much height escapes from top)
		scrollPosition = std::min<unsigned int>(std::max<signed int>(0, tempSignedScroll), maxScroll);
		// refresh scrollbar visuals
		updateScrollBar();
		// set new visibility mask
		visibilityMask = newVis;
	}
public:
	ChatBox() : Drawable(2106632960, Drawable::Flags::Global),
				// stretch 16px at right, top and bottom sides so the edge frame only shows on left
				backPanel(SCREEN_TARGET_WIDTH, -panelBleed, CHAT_TARGET_WIDTH+panelBleed, SCREEN_TARGET_HEIGHT+panelBleed*2, Drawable::Flags::Global),
				scrollBox(0, 0, panelFrame+panelBleed, 0, Drawable::Flags::Global)
		{
		DrawableMgr::Register(this);

		backPanel.SetContents(Bitmap::Create(CHAT_TARGET_WIDTH, SCREEN_TARGET_HEIGHT));
		backPanel.SetZ(2106632959);

		scrollBox.SetZ(2106632960);
		scrollBox.SetVisible(false);

		// create caret (type cursor) graphic
		std::string caretChar = "｜";
		const unsigned int caretLeftKerning = 6;
		auto cRect = Font::Default()->GetSize(caretChar);
		typeCaret = Bitmap::Create(cRect.width+1-caretLeftKerning, cRect.height+1, true);
		Text::Draw(*typeCaret, -caretLeftKerning, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, caretChar);

		// create "name:" prompt graphic
		buildLabelGraphic("Name");
	}

	void Draw(Bitmap& dst) {
		/*
			draw chat log
		*/
		// how much of the log is visible. Depends on whether or not the type box is active
		const unsigned int logVisibleHeight = getLogVisibleHeight();
		int nextHeight = -scrollPosition; // y offset to draw next message, from bottom of log panel
		unsigned int nMessages = messages.size();
		for(int i = nMessages-1; i >= 0; i--) {
			if(!messageVisible(messages[i], visibilityMask)) continue; // skip drawing hidden messages
			BitmapRef mb = messages[i].renderGraphic;
			auto rect = mb->GetRect();
			nextHeight += rect.height;
			if(nextHeight <= 0) continue; // skip drawing offscreen messages, but still accumulate y offset (bottom offscreen)
			Rect cutoffRect = Rect(rect.x, rect.y, rect.width, std::min<unsigned int>(rect.height, nextHeight)); // don't let log be drawn over type box region
			if(messages[i].dirty) {
				unsigned int dummyRef;
				mb = messages[i].renderGraphic = buildMessageGraphic(messages[i].colorA, messages[i].colorB, messages[i].colorC, dummyRef); // redraw message graphic if needed, and if visible
				messages[i].dirty = false;
			}
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+messageMargin, logVisibleHeight-nextHeight, *mb, cutoffRect, Opacity::Opaque());
			if(nextHeight > logVisibleHeight) break; // stop drawing offscreen messages (top offscreen)
		}
		/*
			draw type text
		*/
		if(focused) {
			const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
			const unsigned int typeVisibleWidth = CHAT_TARGET_WIDTH-panelFrame-typePaddingHorz*2-namePad;
			auto rect = typeText->GetRect();
			Rect cutoffRect = Rect(typeScroll, rect.y, std::min<int>(typeVisibleWidth, rect.width-typeScroll), rect.height); // crop type text to stay within padding
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+namePad+typePaddingHorz, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *typeText, cutoffRect, Opacity::Opaque());

			// draw caret
			dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+namePad+typePaddingHorz+typeCharOffsets[typeCaretIndex]-typeScroll, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *typeCaret, typeCaret->GetRect(), Opacity::Opaque());

			// draw name prompt if necessary
			if(nameInputEnabled) {
				dst.Blit(SCREEN_TARGET_WIDTH+panelFrame+typePaddingHorz, SCREEN_TARGET_HEIGHT-typeHeight+typePaddingVert, *namePrompt, namePrompt->GetRect(), Opacity::Opaque());
			}
		}
	}

	void updateTypeText(std::u32string t) {
		// get char offsets for each character in type box, for caret positioning
		typeCharOffsets.clear();
		Rect accumulatedRect;
		const unsigned int nChars = t.size();
		for(int i = 0; i <= nChars; i++) {
			// for every substring of sizes 0 to N, inclusive, starting at the left
			std::u32string textSoFar = t.substr(0, i);
			accumulatedRect = Font::Default()->GetSize(Utils::EncodeUTF(textSoFar)); // absolute offset of character at this point (considering kerning of all previous ones)
			typeCharOffsets.push_back(accumulatedRect.width);
		}

		// final value assigned to accumulatedRect is whole type string
		// create Bitmap graphic for text
		typeText = Bitmap::Create(accumulatedRect.width+1, accumulatedRect.height+1, true);
		Text::Draw(*typeText, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, Utils::EncodeUTF(t));
	}

	void seekCaret(unsigned int i) {
		typeCaretIndex = i;
		// adjust type box horizontal scrolling based on caret position (always keep it in-bounds)
		const unsigned int namePad = nameInputEnabled ? namePromptMargin : 0;
		const unsigned int typeVisibleWidth = CHAT_TARGET_WIDTH-panelFrame-typePaddingHorz*2-namePad;
		const unsigned int caretOffset = typeCharOffsets[typeCaretIndex]; // absolute offset of caret in relation to type text contents
		const int relativeOffset = caretOffset-typeScroll; // caret's position relative to viewable portion of type box
		if(relativeOffset < 0) {
			// caret escapes from left side. adjust
			typeScroll += relativeOffset;
		} else if(relativeOffset >= typeVisibleWidth) {
			// caret escapes from right side. adjust
			typeScroll += relativeOffset-typeVisibleWidth;
		}
	}

	void showNameInput(std::string label) {
		nameInputEnabled = (label != "");
		if(nameInputEnabled) buildLabelGraphic(label);
		updateTypeBox();
	}

	void updateSkin() {
		auto skin = Cache::SystemOrBlack();
		backPanel.SetWindowskin(skin);
		scrollBox.SetWindowskin(skin);
		const unsigned int nMsgs = messages.size();
		for(int i = 0; i < nMsgs; i++) {
			messages[i].dirty = true; // all messages now need to be redrawn with different UI skin
		}
	}

	//

	void appendMessage(std::string colorA, std::string colorB, std::string colorC, bool global) {
		unsigned int graphicHeight;
		BitmapRef text_img = buildMessageGraphic(colorA, colorB, colorC, graphicHeight);
		// append message
		Message _m = {text_img, colorA, colorB, colorC, (unsigned short)(global?Message::VIS_GLOBAL:Message::VIS_LOCAL)};
		messages.push_back(_m);
		if(messageVisible(_m, visibilityMask)) scrollContentHeight += (graphicHeight+1);
		updateScrollBar();
	}

	void scrollUp() {
		// scroll up and assert bounds
		const unsigned int logVisibleHeight = getLogVisibleHeight();
		const unsigned int maxScroll = std::max<int>(0, scrollContentHeight-logVisibleHeight); // maximum value for scrollPosition (how much height escapes from top)
		scrollPosition += 4;
		scrollPosition = std::min<unsigned int>(scrollPosition, maxScroll);
		updateScrollBar();
	}

	void scrollDown() {
		// scroll down and assert bounds
		scrollPosition -= std::min<int>(4, scrollPosition); // minimum value for scrollPosition is 0
		updateScrollBar();
	}

	void onFocus() {
		focused = true;
		updateTypeBox();
		//enable scrollbar
		scrollBox.SetVisible(true);
		updateScrollBar();
	}

	void onUnfocus() {
		focused = false;
		updateTypeBox();
		//disable scrollbar
		scrollBox.SetVisible(false);
		//reset scroll
		scrollPosition = 0;
	}

	void toggleGlobal() {
		changeVisibilityMask(visibilityMask ^ Message::VIS_GLOBAL);
		// TO-DO: some kind of non-intrusive indicator for whether global chat is enabled or disabled.
		// Appending a new message to indicate status bloats chat and doesn't work well with message anchoring.
		//appendMessage("Global messages now ", "", globalChatVisible?"VISIBLE.":"HIDDEN.", false);
	}
};

static const unsigned int MAXCHARSINPUT_NAME = 8;
static const unsigned int MAXCHARSINPUT_TRIPCODE = 256;
static const unsigned int MAXCHARSINPUT_MESSAGE = 200;

static std::unique_ptr<ChatBox> chatBox; //chat renderer
static std::u32string typeText;
static unsigned int typeCaretIndex = 0;
static unsigned int maxChars = MAXCHARSINPUT_NAME;

// TODO: have name and tripcode be on the same step (one typebox under another)
static std::string cacheName = ""; // name and tripcode are input in separate steps. Save it to send them together.

static void createChatWindow(std::shared_ptr<Scene>& scene_map) {
	//select map's scene as current
	auto old_list = &DrawableMgr::GetLocalList();
	DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
	//append new chat window (added to current scene via constructor)
	chatBox = std::make_unique<ChatBox>();
	chatBox->appendMessage("[TAB]: ", "focus/unfocus.", "", false);
	chatBox->appendMessage("[↑, ↓]: ", "scroll.", "", false);
	chatBox->appendMessage("[F8]: ", "hide/show global chat.", "", false);
	chatBox->appendMessage("", "", "―――", false);
	chatBox->appendMessage("• Type /help to list commands.", "", "", false);
	chatBox->appendMessage("• Use '!' at the bebining of", "", "", false);
	chatBox->appendMessage("  message for global chat.", "", "", false);
	chatBox->appendMessage("", "", "―――", false);
	chatBox->appendMessage("• Set a nickname", "", "", false);
	chatBox->appendMessage("  for chat.", "", "", false);
	chatBox->appendMessage("• Max 8 characters.", "", "", false);
	chatBox->appendMessage("• Alphanumeric only.", "", "", false);
	//restore previous list
	DrawableMgr::SetLocalList(old_list);
}

void Chat_Multiplayer::tryCreateChatWindow() { // initialize if haven't already. Invoked by game_multiplayer.cpp
	//create if window not created and map already loaded (i.e. first map to load)
	if(chatBox.get() == nullptr) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if(scene_map == nullptr) {
			Output::Debug("unexpected");
		} else {
			createChatWindow(scene_map);
		}
	} else {
		chatBox->updateSkin(); // refresh skin to new cached one
	}
}

void Chat_Multiplayer::gotMessage(std::string name, std::string trip, std::string msg, std::string src) {
	if(chatBox.get() == nullptr) return; // chatbox not initialized yet
	chatBox->appendMessage((src=="G"?"G← ":"")+name, "•"+trip+":\n", "  "+msg, (src=="G"));
}

void Chat_Multiplayer::gotInfo(std::string msg) {
	if(chatBox.get() == nullptr) return; // chatbox not initialized yet
	// remove leading spaces. TO-DO: fix leading spaces being sent by server on info messages.
	auto leadingSpaces = msg.find_first_not_of(' ');
	auto trim = msg.substr(leadingSpaces != std::string::npos ? leadingSpaces : 0);
	//
	chatBox->appendMessage("", trim, "", false);
}

void Chat_Multiplayer::focus() {
	if(chatBox.get() != nullptr) {
		chatBox->onFocus();
	} else {
		Input::setGameFocus(true);
	}
}

static void unfocus() {
	if(chatBox.get() != nullptr) chatBox->onUnfocus();
	Input::setGameFocus(true);
}

void Chat_Multiplayer::processInputs() {
	if(Input::IsExternalTriggered(Input::InputButton::CHAT_UNFOCUS)) {
		unfocus();
	} else {
		if(chatBox.get() == nullptr) return; // chatbox not initialized yet
		/*
			scroll
		*/
		if(Input::IsExternalPressed(Input::InputButton::CHAT_UP)) {
			chatBox->scrollUp();
		}
		if(Input::IsExternalPressed(Input::InputButton::CHAT_DOWN)) {
			chatBox->scrollDown();
		}
		/*
			options
		*/
		if(Input::IsExternalTriggered(Input::InputButton::CHAT_TOGGLE_GLOBAL)) {
			chatBox->toggleGlobal();
		}
		/*
			typing
		*/
		// input
		std::string inputText = Input::getExternalTextInput();
		if(inputText.size() > 0) {
			std::u32string inputU32 = Utils::DecodeUTF32(inputText);
			std::u32string fits = inputU32.substr(0, maxChars-typeText.size());
			typeText.insert(typeCaretIndex, fits);
			typeCaretIndex += fits.size();
		}
		// erase
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_BACKWARD)) {
			if(typeCaretIndex > 0) typeText.erase(--typeCaretIndex, 1);
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_FORWARD)) {
			typeText.erase(typeCaretIndex, 1);
		}
		// move caret
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_LEFT)) {
			if(typeCaretIndex > 0) typeCaretIndex--;
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_RIGHT)) {
			if(typeCaretIndex < typeText.size()) typeCaretIndex++;
		}
		// update type box
		chatBox->updateTypeText(typeText);
		chatBox->seekCaret(typeCaretIndex);
		/*
			send
		*/
		if(Input::IsExternalTriggered(Input::InputButton::CHAT_SEND)) {
			if(multiplayer__my_name == "") { // name not set, type box should send name
				if(cacheName == "") { //inputting name
					// validate name. 
					// TODO: Server also validates name, but client should receive confirmation from it
					// instead of performing an equal validation
					std::string utf8text = Utils::EncodeUTF(typeText);
					std::regex reg("^[A-Za-z0-9]+$");
					if(	typeText.size() > 0 &&
						typeText.size() <= MAXCHARSINPUT_NAME &&
						std::regex_match(utf8text, reg)	) {
						// name valid
						cacheName = utf8text;
						// reset typebox
						typeText.clear();
						typeCaretIndex = 0;
						chatBox->updateTypeText(typeText);
						chatBox->seekCaret(typeCaretIndex);
						//unfocus();
						// change chatbox state to accept trip
						chatBox->showNameInput("Trip");
						maxChars = MAXCHARSINPUT_TRIPCODE;
						// append tripcode instructions
						chatBox->appendMessage("• Set a tripcode.", "", "", false);
						chatBox->appendMessage("• Leave empty for random.", "", "", false);
						chatBox->appendMessage("• Use it to authenticate", "", "", false);
						chatBox->appendMessage("  yourself.", "", "", false);
					}
				} else { //inputting trip
					// send
					EM_ASM({ SendProfileInfo(UTF8ToString($0), UTF8ToString($1)); }, cacheName.c_str(), Utils::EncodeUTF(typeText).c_str());
					// reset typebox
					typeText.clear();
					typeCaretIndex = 0;
					chatBox->updateTypeText(typeText);
					chatBox->seekCaret(typeCaretIndex);
					//unfocus();
					// change chatbox state to allow for chat
					chatBox->showNameInput("");
					maxChars = MAXCHARSINPUT_MESSAGE;
				}
			} else { // else it's used for sending messages
				SendChatMessage(Utils::EncodeUTF(typeText).c_str());
				// reset typebox
				typeText.clear();
				typeCaretIndex = 0;
				chatBox->updateTypeText(typeText);
				chatBox->seekCaret(typeCaretIndex);
				//unfocus();
			}
		}
	}
}