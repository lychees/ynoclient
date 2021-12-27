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

namespace {
	enum VisibilityType {
		CV_LOCAL =	1,
		CV_GLOBAL =	2
	};

	struct ChatEntry {
		std::string colorA;
		std::string colorB;
		std::string colorC;
		VisibilityType visibility;
		ChatEntry(std::string a, std::string b, std::string c, VisibilityType v) {
			colorA = a;
			colorB = b;
			colorC = c;
			visibility = v;
		}
	};

	////////////////
	////////////////

	class DrawableTypeBox : public Drawable {
		Rect BOUNDS;
		//design parameters
		const unsigned int typeBleed = 3; // amount that is visible outside the padded bounds (so text can be seen beyond a left or rightmost placed caret)
		const unsigned int typePaddingHorz = 9; // padding between type box edges and content (left)
		const unsigned int typePaddingVert = 6; // padding between type box edges and content (top)
		const unsigned int labelPaddingHorz = 8; // left margin between label text and bounds
		const unsigned int labelPaddingVert = 6; // top margin between label text and bounds
		const unsigned int labelMargin = 40; // left margin for type box to make space for the label

		BitmapRef typeText;
		BitmapRef label;
		BitmapRef caret;
		unsigned int caretIndexTail = 0;
		unsigned int caretIndexHead = 0;
		std::vector<unsigned int> typeCharOffsets; // cumulative x offsets for each character in the type box. Used for rendering the caret
		unsigned int scroll = 0; // horizontal scrolling of type box

		unsigned int getLabelMargin() {
			return (label->GetRect().width>1) ? labelMargin : 0;
		}
	public:
		DrawableTypeBox(int x, int y, int w, int h) : Drawable(2106632960, Drawable::Flags::Global), BOUNDS(x, y, w, h) {
			DrawableMgr::Register(this);

			// create caret (type cursor) graphic
			std::string caretChar = "｜";
			const unsigned int caretLeftKerning = 6;
			auto cRect = Font::Default()->GetSize(caretChar);
			caret = Bitmap::Create(cRect.width+1-caretLeftKerning, cRect.height+1, true);
			Text::Draw(*caret, -caretLeftKerning, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, caretChar);

			//initialize
			updateTypeText(std::u32string());
			setLabel("");
		}

		void Draw(Bitmap& dst) {
			const unsigned int labelPad = getLabelMargin();
			const unsigned int typeVisibleWidth = BOUNDS.width-typePaddingHorz*2-labelPad;
			auto rect = typeText->GetRect();
			Rect cutoffRect = Rect(scroll-typeBleed, rect.y, std::min<int>(typeVisibleWidth, rect.width-scroll)+typeBleed*2, rect.height); // crop type text to stay within padding

			// draw contents
			dst.Blit(BOUNDS.x+labelPad+typePaddingHorz-typeBleed, BOUNDS.y+typePaddingVert, *typeText, cutoffRect, Opacity::Opaque());
			// draw caret
			dst.Blit(BOUNDS.x+labelPad+typePaddingHorz+typeCharOffsets[caretIndexHead]-scroll, BOUNDS.y+typePaddingVert, *caret, caret->GetRect(), Opacity::Opaque());
			// draw label
			dst.Blit(BOUNDS.x+labelPaddingHorz, BOUNDS.y+labelPaddingVert, *label, label->GetRect(), Opacity::Opaque());
			// draw selection
			const unsigned int caretStart = std::min<unsigned int>(caretIndexTail, caretIndexHead);
			const unsigned int caretEnd = std::max<unsigned int>(caretIndexTail, caretIndexHead);
			const unsigned int selectStart = BOUNDS.x+labelPad+typePaddingHorz+std::max<int>(typeCharOffsets[caretStart]-scroll, -typeBleed);
			const unsigned int selectEnd = BOUNDS.x+labelPad+typePaddingHorz+std::min<int>(typeCharOffsets[caretEnd]-scroll, typeVisibleWidth+typeBleed);
			Rect selectedRect = Rect(selectStart, BOUNDS.y+typePaddingVert, selectEnd-selectStart, BOUNDS.height-typePaddingVert*2);
			dst.FillRect(selectedRect, Color(255, 255, 255, 100));
		};

		void refreshTheme() { }

		void updateTypeText(std::u32string text) {
			// get char offsets for each character in type box, for caret positioning
			typeCharOffsets.clear();
			Rect accumulatedRect;
			const unsigned int nChars = text.size();
			for(int k = 0; k <= nChars; k++) {
				// for every substring of sizes 0 to N, inclusive, starting at the left
				std::u32string textSoFar = text.substr(0, k);
				accumulatedRect = Font::Default()->GetSize(Utils::EncodeUTF(textSoFar)); // absolute offset of character at this point (considering kerning of all previous ones)
				typeCharOffsets.push_back(accumulatedRect.width);
			}
			
			// final value assigned to accumulatedRect is whole type string
			// create Bitmap graphic for text
			typeText = Bitmap::Create(accumulatedRect.width+1, accumulatedRect.height+1, true);
			Text::Draw(*typeText, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 0, Utils::EncodeUTF(text));
		}

		void seekCaret(unsigned int seekTail, unsigned int seekHead) {
			caretIndexTail = seekTail;
			caretIndexHead = seekHead;
			// adjust type box horizontal scrolling based on caret position (always keep it in-bounds)
			const unsigned int labelPad = getLabelMargin();
			const unsigned int typeVisibleWidth = BOUNDS.width-typePaddingHorz*2-labelPad;
			const unsigned int caretOffset = typeCharOffsets[caretIndexHead]; // absolute offset of caret in relation to type text contents
			const int relativeOffset = caretOffset-scroll; // caret's position relative to viewable portion of type box
			if(relativeOffset < 0) {
				// caret escapes from left side. adjust
				scroll += relativeOffset;
			} else if(relativeOffset >= typeVisibleWidth) {
				// caret escapes from right side. adjust
				scroll += relativeOffset-typeVisibleWidth;
			}
		}

		void setLabel(std::string l) {
			std::string prompt = (l!="")?l+":":"";
			auto lRect = Font::Default()->GetSize(prompt);
			label = Bitmap::Create(lRect.width+1, lRect.height+1, true);
			Text::Draw(*label, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 1, prompt);
		}

		Rect getFormBounds() {
			const unsigned int labelPad = getLabelMargin();
			return Rect(BOUNDS.x+labelPad, BOUNDS.y, BOUNDS.width-labelPad, BOUNDS.height);
		}
	};

	////////////////
	////////////////

	class DrawableOnlineStatus : public Drawable {
		Rect BOUNDS;
		//design parameters
		const unsigned int paddingHorz = 8; // padding between box edges and content (left)
		const unsigned int paddingVert = 6; // padding between box edges and content (top)

		BitmapRef connStatus;
		BitmapRef roomStatus;
	public:
		DrawableOnlineStatus(int x, int y, int w, int h) : Drawable(2106632960, Drawable::Flags::Global), BOUNDS(x, y, w, h) {
			DrawableMgr::Register(this);

			//initialize
			setConnectionStatus(false);
			setRoomStatus(0);
		}

		void Draw(Bitmap& dst) {
			dst.Blit(BOUNDS.x+paddingHorz, BOUNDS.y+paddingVert, *connStatus, connStatus->GetRect(), Opacity::Opaque());
			auto rRect = roomStatus->GetRect();
			dst.Blit(BOUNDS.x+BOUNDS.width-paddingHorz-rRect.width, BOUNDS.y+paddingVert, *roomStatus, rRect, Opacity::Opaque());
		};

		void refreshTheme() { }

		void setConnectionStatus(bool status) {
			std::string connLabel = (status)?"Connected":"Disconnected";
			auto cRect = Font::Default()->GetSize(connLabel);
			connStatus = Bitmap::Create(cRect.width+1, cRect.height+1, true);
			Text::Draw(*connStatus, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 2, connLabel);
		}

		void setRoomStatus(unsigned int roomID) {
			std::string roomLabel = "Room #"+std::to_string(roomID);
			auto rRect = Font::Default()->GetSize(roomLabel);
			roomStatus = Bitmap::Create(rRect.width+1, rRect.height+1, true);
			Text::Draw(*roomStatus, 0, 0, *Font::Default(), *Cache::SystemOrBlack(), 2, roomLabel);
		}
	};

	////////////////
	////////////////

	class DrawableChatLog : public Drawable {
		struct DrawableChatEntry {
			ChatEntry* messageData;
			BitmapRef renderGraphic;
			bool dirty; // need to redraw? (for when UI skin changes)
			DrawableChatEntry(ChatEntry* msg) {
				messageData = msg;
				renderGraphic = nullptr;
				dirty = true;
			}
		};

		Rect BOUNDS;
		//design parameters
		const unsigned int messageMargin = 3; // horizontal margin between panel edges and content
		const unsigned int scrollFrame = 4; // width of scroll bar's visual frame (on right side)
		const unsigned int scrollBleed = 16; // how much to stretch right edge of scroll box offscreen (so only left frame shows)

		Window_Base scrollBox; // box used as rendered design for a scrollbar
		std::vector<DrawableChatEntry> messages;
		int scrollPosition = 0;
		unsigned int scrollContentHeight = 0; // total height of scrollable message log
		unsigned short visibilityFlags = CV_LOCAL | CV_GLOBAL;

		void buildMessageGraphic(DrawableChatEntry& msg) {
			// manual text wrapping
			const unsigned int maxWidth = BOUNDS.width-scrollFrame-messageMargin*2;
			std::vector<std::pair<std::string, unsigned int>> lines; // individual lines saved so far, along with their y offset
			unsigned int totalWidth = 0; // maximum width between all lines
			unsigned int totalHeight = 0; // accumulated height from all lines

			std::string colorA = msg.messageData->colorA;
			std::string colorB = msg.messageData->colorB;
			std::string colorC = msg.messageData->colorC;
			std::string currentLine = colorA+colorB+colorC; // current line being worked on. start with whole message.
			std::string nextLine = ""; // stores characters moved down to line below
			do {
				auto rect = Font::Tiny()->GetSize(currentLine);
				while(rect.width > maxWidth) {
					// as long as current line exceeds maximum width,
					// move one word from this line down to the next one
					unsigned int lastSpace = currentLine.find_last_of(' ');
					if(lastSpace != std::string::npos && lastSpace < currentLine.size()-1) {
						// there is a word that can be moved down
						nextLine = currentLine.substr(lastSpace+1, std::string::npos)+nextLine;
						currentLine = currentLine.substr(0, lastSpace+1);
					} else {
						// there is not a whole word that can be moved down, so move individual characters.
						// this case happens when last character in current line is a space, 
						// or when there are no spaces in the current line
						nextLine = currentLine.back()+nextLine;
						currentLine.pop_back();
					}
					// recalculate line width with characters having been moved down
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
			msg.renderGraphic = text_img;
			msg.dirty = false;
		}

		void assertScrollBounds() {
			const unsigned int maxScroll = std::max<int>(0, scrollContentHeight-BOUNDS.height); // maximum value for scrollPosition (= amount of height escaping from the top)
			scrollPosition = std::max<int>(scrollPosition, 0);
			scrollPosition = std::min<int>(scrollPosition, maxScroll);
		}

		void updateScrollBar() {
			if(scrollContentHeight <= BOUNDS.height) {
				scrollBox.SetX(BOUNDS.x+BOUNDS.width); // hide scrollbar if content isn't large enough for scroll
				return;
			}
			scrollBox.SetX(BOUNDS.x+BOUNDS.width-scrollFrame); // show scrollbar
			// position scrollbar
			const float ratio = BOUNDS.height/float(scrollContentHeight);
			const unsigned int barHeight = BOUNDS.height*ratio;
			const unsigned int barY = scrollPosition*ratio;
			scrollBox.SetHeight(barHeight);
			scrollBox.SetY(BOUNDS.y+BOUNDS.height-barY-barHeight);
		}

		// called when:
		//	- scroll position changes
		//	- content height changes
		//	- visible height (BOUNDS.height) changes
		void refreshScroll() {
			assertScrollBounds();
			updateScrollBar();
		}

		bool messageVisible(DrawableChatEntry& msg, unsigned short v) {
			return (msg.messageData->visibility & v) > 0;
		}
	public:
		DrawableChatLog(int x, int y, int w, int h) : Drawable(2106632960, Drawable::Flags::Global), BOUNDS(x, y, w, h), scrollBox(0, 0, scrollFrame+scrollBleed, 0, Drawable::Flags::Global) {
			DrawableMgr::Register(this);

			scrollBox.SetZ(2106632960);
			scrollBox.SetVisible(false);
		}

		void setHeight(unsigned int h) {
			BOUNDS.height = h;
			refreshScroll();
		}

		void Draw(Bitmap& dst) {
			int nextHeight = -scrollPosition; // y offset to draw next message, from bottom of log panel
			unsigned int nMessages = messages.size();
			for(int i = nMessages-1; i >= 0; i--) {
				DrawableChatEntry& dmsg = messages[i];
				//skip drawing hidden messages
				if(!messageVisible(dmsg, visibilityFlags)) continue;
				//
				auto rect = dmsg.renderGraphic->GetRect();
				// accumulate y offset
				nextHeight += rect.height;
				// skip drawing offscreen messages, but still accumulate y offset (bottom offscreen)
				if(nextHeight <= 0) continue;
				// cutoff message graphic so text does not bleed out of bounds
				const unsigned int topOffscreen = std::max<int>(nextHeight-BOUNDS.height, 0);
				Rect cutoffRect = Rect(rect.x, rect.y+topOffscreen, rect.width, std::min<unsigned int>(rect.height, nextHeight)-topOffscreen);
				// rebuild message graphic if needed
				if(messages[i].dirty) buildMessageGraphic(dmsg);
				//draw
				dst.Blit(BOUNDS.x+messageMargin, BOUNDS.y+BOUNDS.height-nextHeight+topOffscreen, *(dmsg.renderGraphic), cutoffRect, Opacity::Opaque());
				// stop drawing offscreen messages (top offscreen)
				if(nextHeight > BOUNDS.height) break;
			}
		};

		void refreshTheme() {
			scrollBox.SetWindowskin(Cache::SystemOrBlack());
			for(int i = 0; i < messages.size(); i++) {
				messages[i].dirty = true; // all messages now need to be redrawn with different UI skin
			}
		}

		void addChatEntry(ChatEntry* messageData) {
			DrawableChatEntry dMsg = DrawableChatEntry(messageData);
			buildMessageGraphic(dMsg);
			messages.push_back(dMsg);

			if(messageVisible(dMsg, visibilityFlags)) {
				scrollContentHeight += dMsg.renderGraphic->GetRect().height;
				refreshScroll();
			}
		}

		void removeChatEntry(ChatEntry* messageData) {
			unsigned int nMsgs = messages.size();
			for(int i = 0; i < nMsgs; i++) {
				DrawableChatEntry& dMsg = messages[i];
				if(dMsg.messageData == messageData) {
					if(messageVisible(dMsg, visibilityFlags)) {
						scrollContentHeight -= dMsg.renderGraphic->GetRect().height;
						refreshScroll();
					}
					messages.erase(messages.begin()+i);
					break;
				}
			}
		}

		void setScroll(int s) {
			scrollPosition = s;
			refreshScroll();
		}

		void scroll(int delta) {
			setScroll(scrollPosition+delta);
		}

		void showScrollBar(bool v) {
			scrollBox.SetVisible(v);
		}

		void toggleVisibilityFlag(VisibilityType v) {
			// Expands/collapses messages in-place, 
			// so you don't get lost if you've scrolled far up.
			//
			// Finds the bottommost (before the change) message that is visible both before and after changing visibility flags, and
			// anchors it into place so it stays at the same visual location before and
			// after expanding/collapsing.

			unsigned short newVisibilityFlags = visibilityFlags ^ v;
			// calculate new total content height for new visibility flags.
			unsigned int newContentHeight = 0;
			// save anchor position before and after
			int preAnchorY = -scrollPosition;
			int postAnchorY = -scrollPosition;
			bool anchored = false; // if true, anchor has been found, so stop accumulating message heights
			//
			for(int i = messages.size()-1; i >= 0; i--) {
				bool preVis = messageVisible(messages[i], visibilityFlags); // is message visible with previous visibility mask?
				bool postVis = messageVisible(messages[i], newVisibilityFlags); // is message visible with new visibility mask?
				unsigned int msgHeight = messages[i].renderGraphic->GetRect().height;
				// accumulate total content height for new visibility flags
				if(postVis) newContentHeight += msgHeight;

				if(!anchored) {
					if(preVis) preAnchorY += msgHeight;
					if(postVis) postAnchorY += msgHeight;
					bool validAnchor = preVis && postVis; // this message is an anchor candidate since it is visible both before and after
					if(validAnchor && preAnchorY > 0) {
						anchored = true; // this is the bottommost anchorable message
					}
				}
			}
			// updates scroll content height
			scrollContentHeight = newContentHeight;
			// adjusts scroll position so anchor stays in place
			int scrollDelta = postAnchorY-preAnchorY;
			scrollPosition += scrollDelta;
			refreshScroll();
			// set new visibility mask
			visibilityFlags = newVisibilityFlags;
		}
	};

	////////////////
	////////////////

	class DrawableChat : public Drawable {
		//design parameters
		const unsigned int panelBleed = 16; // how much to stretch top, bottom and right edges of panel offscreen (so only left frame shows)
		const unsigned int panelFrame = 4; // width of panel's visual frame (on left side)
		const unsigned int statusHeight = 24; // height of status region on top of chatlog
		const unsigned int typeHeight = 24; // height of type box
		const unsigned int fullscreenIconMargin = 10; // right margin for status text to make space for fullscreen toggle icon

		DrawableChatLog dLog;
		DrawableTypeBox dType;
		DrawableOnlineStatus dStatus;
		Window_Base backPanel; // background pane

		void updateTypePanel() {
			if(dType.IsVisible()) {
				const unsigned int f = -8; // SetCursorRect for some reason already has a padding of 8px relative to the window, so we fix it
				const auto formRect = dType.getFormBounds();
				backPanel.SetCursorRect(Rect(f+formRect.x-SCREEN_TARGET_WIDTH, f+formRect.y+panelBleed, formRect.width, formRect.height));
			} else {
				backPanel.SetCursorRect(Rect(0, 0, 0, 0));
			}
		}
	public:
		DrawableChat() : Drawable(2106632960, Drawable::Flags::Global),
		// stretch 16px at right, top and bottom sides so the edge frame only shows on left
		backPanel(SCREEN_TARGET_WIDTH, -panelBleed, CHAT_TARGET_WIDTH+panelBleed, SCREEN_TARGET_HEIGHT+panelBleed*2, Drawable::Flags::Global),
		dLog(SCREEN_TARGET_WIDTH+panelFrame, statusHeight, CHAT_TARGET_WIDTH-panelFrame, SCREEN_TARGET_HEIGHT-statusHeight),
		dType(SCREEN_TARGET_WIDTH+panelFrame, SCREEN_TARGET_HEIGHT-typeHeight, CHAT_TARGET_WIDTH-panelFrame, typeHeight),
		dStatus(SCREEN_TARGET_WIDTH+panelFrame, 0, CHAT_TARGET_WIDTH-panelFrame-fullscreenIconMargin, statusHeight)
		{
			DrawableMgr::Register(this);

			backPanel.SetZ(2106632959);
			dType.SetVisible(false);
		}

		void Draw(Bitmap& dst) { }

		void addLogEntry(ChatEntry* msg) {
			dLog.addChatEntry(msg);
		}

		void removeLogEntry(ChatEntry* msg) {
			dLog.removeChatEntry(msg);
		}

		void setStatusConnection(bool conn) {
			dStatus.setConnectionStatus(conn);
		}

		void setStatusRoom(unsigned int roomID) {
			dStatus.setRoomStatus(roomID);
		}

		void refreshTheme() {
			backPanel.SetWindowskin(Cache::SystemOrBlack());
			dLog.refreshTheme();
			dType.refreshTheme();
			dStatus.refreshTheme();
		}

		void updateTypeBox(std::u32string text, unsigned int caretSeekTail, unsigned int caretSeekHead) {
			dType.updateTypeText(text);
			dType.seekCaret(caretSeekTail, caretSeekHead);
		}

		void showTypeLabel(std::string label) {
			dType.setLabel(label);
			updateTypePanel();
		}

		void scrollUp() {
			dLog.scroll(+4);
		}

		void scrollDown() {
			dLog.scroll(-4);
		}

		void setFocus(bool focused) {
			dType.SetVisible(focused);
			updateTypePanel();
			dLog.showScrollBar(focused);
			if(focused) {
				dLog.setHeight(SCREEN_TARGET_HEIGHT-statusHeight-typeHeight);
			} else {
				dLog.setHeight(SCREEN_TARGET_HEIGHT-statusHeight);
				dLog.setScroll(0);
			}
		}

		void toggleVisibilityFlag(VisibilityType v) {
			dLog.toggleVisibilityFlag(v);
		}
	};

	////////////////
	////////////////

	const unsigned int MAXCHARSINPUT_NAME = 8;
	const unsigned int MAXCHARSINPUT_TRIPCODE = 256;
	const unsigned int MAXCHARSINPUT_MESSAGE = 200;

	const unsigned int MAXMESSAGES = 100;

	std::u32string typeText;
	unsigned int typeCaretIndexTail = 0; // anchored when SHIFT-selecting text
	unsigned int typeCaretIndexHead = 0; // moves when SHIFT-selecting text
	unsigned int typeMaxChars = MAXCHARSINPUT_NAME;
	// TODO: have name and tripcode be on the same step (one typebox under another)
	std::string cacheName = ""; // name and tripcode are input in separate steps. Save it to send them together.
	std::u32string preloadTrip; // saved tripcode preference to load into trip type box once name has been sent.
	std::unique_ptr<DrawableChat> chatBox; //chat renderer
	std::vector<std::unique_ptr<ChatEntry>> chatLog;

	void addLogEntry(std::string a, std::string b, std::string c, VisibilityType v) {
		chatLog.push_back(std::make_unique<ChatEntry>(a, b, c, v));
		chatBox->addLogEntry(chatLog.back().get());
		if(chatLog.size() > MAXMESSAGES) {
			chatBox->removeLogEntry(chatLog.front().get());
			chatLog.erase(chatLog.begin());
		}
	}

	void initialize() {
		chatBox = std::make_unique<DrawableChat>();

		chatBox->showTypeLabel("Name");

		// load saved user profile preferences from JS side (name)
		char* configNameStr = (char*)EM_ASM_INT({
			var str = getProfileConfigName();
			var len = lengthBytesUTF8(str)+1;
			var wasmStr = _malloc(len);
			stringToUTF8(str, wasmStr, len);
			return wasmStr;
		});
		std::string cfgNameStr = configNameStr;
		typeText = Utils::DecodeUTF32(cfgNameStr);
		typeCaretIndexTail = typeCaretIndexHead = typeText.size();
		chatBox->updateTypeBox(typeText, typeCaretIndexTail, typeCaretIndexHead);
		free(configNameStr);
		// load saved user profile preferences from JS side (trip)
		char* configTripStr = (char*)EM_ASM_INT({
			var str = getProfileConfigTrip();
			var len = lengthBytesUTF8(str)+1;
			var wasmStr = _malloc(len);
			stringToUTF8(str, wasmStr, len);
			return wasmStr;
		});
		std::string cfgTripStr = configTripStr;
		preloadTrip = Utils::DecodeUTF32(cfgTripStr);
		free(configTripStr);

		addLogEntry("[TAB]: ", "focus/unfocus.", "", CV_LOCAL);
		addLogEntry("[↑, ↓]: ", "scroll.", "", CV_LOCAL);
		addLogEntry("[F8]: ", "hide/show global chat.", "", CV_LOCAL);
		addLogEntry("", "", "―――", CV_LOCAL);
		addLogEntry("• Type /help to list commands.", "", "", CV_LOCAL);
		addLogEntry("• Use '!' at the bebining of", "", "", CV_LOCAL);
		addLogEntry("  message for global chat.", "", "", CV_LOCAL);
		addLogEntry("", "", "―――", CV_LOCAL);
		addLogEntry("• Set a nickname", "", "", CV_LOCAL);
		addLogEntry("  for chat.", "", "", CV_LOCAL);
		addLogEntry("• Max 8 characters.", "", "", CV_LOCAL);
		addLogEntry("• Alphanumeric only.", "", "", CV_LOCAL);
	}

	void setFocus(bool focused) {
		Input::setGameFocus(!focused);
		chatBox->setFocus(focused);
	}

	void inputsFocusUnfocus() {
		if(Input::IsTriggered(Input::InputButton::CHAT_FOCUS)) {
			setFocus(true);
		} else if(Input::IsExternalTriggered(Input::InputButton::CHAT_UNFOCUS)) {
			setFocus(false);
		}
	}

	void inputsLog() {
		if(Input::IsExternalPressed(Input::InputButton::CHAT_UP)) {
			chatBox->scrollUp();
		}
		if(Input::IsExternalPressed(Input::InputButton::CHAT_DOWN)) {
			chatBox->scrollDown();
		}
		if(Input::IsExternalTriggered(Input::InputButton::CHAT_TOGGLE_GLOBAL)) {
			chatBox->toggleVisibilityFlag(CV_GLOBAL);
		}
	}

	void inputsTyping() {
		// input
		std::string inputText = Input::getExternalTextInput();
		if(inputText.size() > 0) {
			unsigned int caretStart = std::min<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			unsigned int caretEnd = std::max<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			typeText.erase(caretStart, caretEnd-caretStart);
			std::u32string inputU32 = Utils::DecodeUTF32(inputText);
			std::u32string fits = inputU32.substr(0, typeMaxChars-typeText.size());
			typeText.insert(caretStart, fits);
			typeCaretIndexTail = typeCaretIndexHead = caretStart+fits.size();
		}
		// erase
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_BACKWARD)) {
			unsigned int caretStart = std::min<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			unsigned int caretEnd = std::max<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			unsigned int effectiveStart = std::max<int>(0, caretEnd-std::max<unsigned int>(1, caretEnd-caretStart));
			typeText.erase(effectiveStart, caretEnd-effectiveStart);
			typeCaretIndexTail = typeCaretIndexHead = effectiveStart;
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_DEL_FORWARD)) {
			unsigned int caretStart = std::min<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			unsigned int caretEnd = std::max<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
			typeText.erase(caretStart, std::max<unsigned int>(1, caretEnd-caretStart));
			typeCaretIndexTail = typeCaretIndexHead = caretStart;
		}
		// move caret
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_LEFT)) {
			if(Input::IsExternalPressed(Input::InputButton::SHIFT)) {
				if(typeCaretIndexHead > 0) typeCaretIndexHead--;
			} else {
				typeCaretIndexHead = std::min<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
				if(typeCaretIndexTail == typeCaretIndexHead && typeCaretIndexHead > 0) typeCaretIndexHead--;
				typeCaretIndexTail = typeCaretIndexHead;
			}
		}
		if(Input::IsExternalRepeated(Input::InputButton::CHAT_RIGHT)) {
			if(Input::IsExternalPressed(Input::InputButton::SHIFT)) {
				if(typeCaretIndexHead < typeText.size()) typeCaretIndexHead++;
			} else {
				typeCaretIndexHead = std::max<unsigned int>(typeCaretIndexTail, typeCaretIndexHead);
				if(typeCaretIndexTail == typeCaretIndexHead && typeCaretIndexHead < typeText.size()) typeCaretIndexHead++;
				typeCaretIndexTail = typeCaretIndexHead;
			}
		}
		// update type box
		// TODO: only update when inputs change type text or caret position
		chatBox->updateTypeBox(typeText, typeCaretIndexTail, typeCaretIndexHead);
		// send
		if(Input::IsExternalTriggered(Input::InputButton::CHAT_SEND)) {
			if(multiplayer__my_name == "") { // name not set, type box should send profile info
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
						// load trip preferences
						typeText = preloadTrip;
						typeCaretIndexTail = typeCaretIndexHead = typeText.size();
						chatBox->updateTypeBox(typeText, typeCaretIndexTail, typeCaretIndexHead);
						// change chatbox state to accept trip
						chatBox->showTypeLabel("Trip");
						typeMaxChars = MAXCHARSINPUT_TRIPCODE;
						// append tripcode instructions
						addLogEntry("• Set a tripcode.", "", "", CV_LOCAL);
						addLogEntry("• Leave empty for random.", "", "", CV_LOCAL);
						addLogEntry("• Use it to authenticate", "", "", CV_LOCAL);
						addLogEntry("  yourself.", "", "", CV_LOCAL);
					}
				} else { //inputting trip
					// send
					EM_ASM({ SendProfileInfo(UTF8ToString($0), UTF8ToString($1)); }, cacheName.c_str(), Utils::EncodeUTF(typeText).c_str());
					// reset typebox
					typeText.clear();
					typeCaretIndexTail = typeCaretIndexHead = 0;
					chatBox->updateTypeBox(typeText, typeCaretIndexTail, typeCaretIndexHead);
					// change chatbox state to allow for chat
					chatBox->showTypeLabel("");
					typeMaxChars = MAXCHARSINPUT_MESSAGE;
				}
			} else { // else it's used for sending messages
				SendChatMessage(Utils::EncodeUTF(typeText).c_str());
				// reset typebox
				typeText.clear();
				typeCaretIndexTail = typeCaretIndexHead = 0;
				chatBox->updateTypeBox(typeText, typeCaretIndexTail, typeCaretIndexHead);
			}
		}
	}

	void processInputs() {
		inputsFocusUnfocus();
		inputsLog();
		inputsTyping();
	}
}

// External access
// ---------------

void Chat_Multiplayer::refresh() {
	if(chatBox == nullptr) {
		initialize();
	} else {
		chatBox->refreshTheme();
	}
}

void Chat_Multiplayer::update() {
	if(chatBox == nullptr) return;
	processInputs();
}

void Chat_Multiplayer::gotMessage(std::string name, std::string trip, std::string msg, std::string src) {
	if(chatBox == nullptr) return;
	addLogEntry(
		(src=="G"?"G← ":"")+name,
		"•"+trip+":\n",
		"\u00A0"+msg,
		src=="G"?CV_GLOBAL:CV_LOCAL
	);
}

void Chat_Multiplayer::gotInfo(std::string msg) {
	if(chatBox == nullptr) return;
	// remove leading spaces. TO-DO: fix leading spaces being sent by server on info messages.
	auto leadingSpaces = msg.find_first_not_of(' ');
	auto trim = msg.substr(leadingSpaces != std::string::npos ? leadingSpaces : 0);
	//
	addLogEntry("", trim, "", CV_LOCAL);
}

void Chat_Multiplayer::setStatusConnection(bool status) {
	if(chatBox == nullptr) return;
	chatBox->setStatusConnection(status);
}

void Chat_Multiplayer::setStatusRoom(unsigned int roomID) {
	if(chatBox == nullptr) return;
	chatBox->setStatusRoom(roomID);
}