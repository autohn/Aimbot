#pragma once

//===============================================================================
class Mouse
{
public:
	Mouse();
	~Mouse();

	void move(int x, int y);
	void down();
	void up();

private:
	static int counter;
	static void Init();
	static void DeInit();
};
