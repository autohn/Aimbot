#pragma once
#include <windows.h>
#include <stdint.h>

class Bitmap
{
public:
	Bitmap() :data(0), owndata(false) {}
	Bitmap(uint8_t* data, int w, int h, int rowpitch) : data(data), width(w), height(h), rowpitch(rowpitch), owndata(false) {}
	~Bitmap() { Release(); }

	inline uint8_t* operator()(int x, int y) { return data + y * rowpitch + x * 4; }
	inline const uint8_t* operator()(int x, int y) const { return data + y * rowpitch + x * 4; }
	inline int w() const { return width; }
	inline int h() const { return height; }
	inline const uint8_t* get_data() const { return data; }
	inline int get_rowpitch() const { return rowpitch;  }

	Bitmap& operator=(const Bitmap& bmp);

	void Keep();
	void Keep(const Bitmap& bmp) { (*this) = bmp; Keep(); }
	void Release() {if (owndata) {delete data; data = 0; owndata = false;}}

	void Save(LPTSTR path) { save(path); }
	void SaveToClipboard() { save(0); }

private:
	uint8_t *data;
	int width, height, rowpitch;
	bool owndata;

	void save(LPTSTR path);
};

