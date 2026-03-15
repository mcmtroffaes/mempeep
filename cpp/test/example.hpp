#pragma once

struct Pos {
	int x;
	int y;
	int z;
};

struct Entity {
	Pos pos;
	int health;
};

struct State {
	int tick;
	Entity player;
	Entity enemy;
};
