class Actor {
public :
    int x,y; // position on map
    int ch; // ascii code
    TCODColor col; // color

    Actor(int x, int y, int ch, const TCODColor &col);
    void render() const;
};


Actor::Actor(int x, int y, int ch, const TCODColor &col) :
    x(x),y(y),ch(ch),col(col) {
}

void Actor::render() const {
    TCODConsole::root->setChar(x,y,ch);
    TCODConsole::root->setCharForeground(x,y,col);
}
