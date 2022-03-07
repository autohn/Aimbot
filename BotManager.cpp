#include "BotManager.h"
#include <stdio.h>
#include <vector>
#include <algorithm>

#include <opencv2/core/ocl.hpp>

using namespace cv;
using namespace std;
using namespace DirectX;

//===============================================================================
enum { B = 2, G = 1, R = 0 };

inline float redStrength(const uint8_t* pixel)
{
	return pixel[R] / ((float)pixel[G] + pixel[B] + 1.f);
}

inline int countRed(const Bitmap& bmp, float rMax, int x, int y, int d)
{
	return	(redStrength(bmp(x + d, y)) > rMax ? 1 : 0) +
		(redStrength(bmp(x - d, y)) > rMax ? 1 : 0) +
		(redStrength(bmp(x, y + d)) > rMax ? 1 : 0) +
		(redStrength(bmp(x, y - d)) > rMax ? 1 : 0) +
		(redStrength(bmp(x + d, y + d)) > rMax ? 1 : 0) +
		(redStrength(bmp(x - d, y - d)) > rMax ? 1 : 0) +
		(redStrength(bmp(x - d, y + d)) > rMax ? 1 : 0) +
		(redStrength(bmp(x + d, y - d)) > rMax ? 1 : 0);
}

BotManager::BotManager() :
	sLastCBTime(0),
	sLastCBCounter(0),
	sFPS(-1),
	sMode(-1),
	sCapsLockState(false), sDoAimBot(false),
	sKey1State(false), sDoTestMouse(false),
	sKey2State(false), sDoSaveScreenshots(false),
	sKey3State(false), WithoutLeftkey(false),
	sKey4State(false), AimType(at_TrackerFirst),
	sAimSpeed(1),
	mShot(0),
	mSave(0),
	lastPx(-1), lastPy(-1)
{}

void BotManager::Process(Bitmap& bmp)
{
	if (((::GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0) != sCapsLockState) //aim
	{
		sCapsLockState = !sCapsLockState;
		if (sCapsLockState)
			sDoAimBot = !sDoAimBot;
	}
	if (((::GetAsyncKeyState('1') & 0x8000) != 0) != sKey1State) // test mouse
	{
		sKey1State = !sKey1State;
		if (sKey1State)
			sDoTestMouse = !sDoTestMouse;
		if (sDoTestMouse && sDoSaveScreenshots)
		{
			mShot++;
			mSave = kBMPToSave;
		}
	}

	if (((::GetAsyncKeyState('2') & 0x8000) != 0) != sKey2State) // save screenshots
	{
		sKey2State = !sKey2State;
		if (sKey2State)
			sDoSaveScreenshots = !sDoSaveScreenshots;
	}

	if (((::GetAsyncKeyState('3') & 0x8000) != 0) != sKey3State) // left key
	{
		sKey3State = !sKey3State;
		if (sKey3State)
			WithoutLeftkey = !WithoutLeftkey;
	}

	if (((::GetAsyncKeyState('4') & 0x8000) != 0) != sKey4State) // aim algorithm
	{
		sKey4State = !sKey4State;
		if (sKey4State)
			AimType = (AimTypeT)((AimType + 1) % at_Last);
	}

	if ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) //L mouse released
		if (tracker) tracker.release();

	AimInfo aim_info;
	bool found = false;
	if (sDoTestMouse)
		//TriggerBot(bmp);
	{
		TestMouse(aim_info.px);
		found = true;
	}
	/*
	else if ((sDoAimBot && ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)))
	{
			found = AimBot(bmp, aim_info, 0);
			if (found && sDoSaveScreenshots && mSave==0)
			{
				mShot++;
				mSave = kBMPToSave;
			}
	}
	*/
	else if ((sDoAimBot && ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)) || (sDoAimBot && WithoutLeftkey))
	{
		found = AimBot(bmp, aim_info, AimType);
		if (found && sDoSaveScreenshots && mSave == 0)
		{
			mShot++;
			mSave = kBMPToSave;
		}
	}

	//TriggerBot(bmp);

	if (mSave > 0)
	{
		mSave--;

		Screenshot& scr = mScr[kBMPToSave - mSave - 1];
		scr.bmp.Keep(bmp);
		scr.time = timeGetTime();
		scr.aim = aim_info;
		scr.found = found;

		if (mSave == 0)
		{
			for (int i = 0; i < kBMPToSave; i++)
			{
				Screenshot& s = mScr[i];
				wchar_t path[MAX_PATH];
				if (s.found)
					swprintf(path, L"c:\\aim\\%d_%d [%d,%d] (%d,%d) %d.bmp", mShot, i, s.aim.px, s.aim.py,
						s.aim.x, s.aim.y, (int)(s.time - mScr[0].time));
				//					swprintf(path, L"c:\\aim\\%d_%d [%d,%d] (%d,%d) {%d,%d} %d.bmp", mShot, i, s.aim.px, s.aim.py,
				//						s.aim.x, s.aim.y, s.aim.dx, s.aim.dy, (int)(s.time - mScr[0].time));
				else
					swprintf(path, L"c:\\aim\\%d_%d [NA] %d.bmp", mShot, i, (int)(s.time - mScr[0].time));

				Bitmap bmp;
				bmp.Keep(s.bmp);
				int sx = 0, sy = 0, cnt = 0;
				int width = bmp.w();
				int height = bmp.h();
				int xSkip = width / 2 - kWorkingAreaHalf;// width / 3;
				int ySkip = height / 2 - kWorkingAreaHalf;// height / 3;

				if (AimType == at_Color)
				{
					for (int x = xSkip; x < s.bmp.w() - xSkip; x++)
						for (int y = ySkip; y < s.bmp.h() - ySkip; y++)
						{
							float red = redStrength(bmp(x, y));
							float rMax = 1.0f;

							bool border = s.aim.border[x - xSkip][y - ySkip];

							int r1;
							if (border)
							{
								sx += x;
								sy += y;
								cnt++;
								uint8_t* px = s.bmp(x, y);
								px[0] = px[1] = 0; px[2] = 255;
							}
							else if (red > rMax && ((r1 = countRed(bmp, rMax, x, y, 1)) == 2 || r1 == 3 && countRed(bmp, rMax, x, y, 2) == 2))
							{
								uint8_t* px = s.bmp(x, y);
								px[0] = 0; px[1] = 255; px[2] = 0;
							}
							else if (red > 0.90f)
							{
								uint8_t* px = s.bmp(x, y);
								px[0] = px[1] = px[2] = min((int)(red * 128), 255);
							}
							else
							{
								uint8_t* px = s.bmp(x, y);
								px[0] = px[1] = px[2] = 0;
							}
						}

					int ss = 0;
					for (int dy = 0; dy < kWorkingAreaHalf * 2; dy++)
					{
						for (int dx = 0; dx < s.aim.sil[dy]; dx++)
						{
							uint8_t* px = s.bmp(s.bmp.w() - xSkip + dx, ySkip + dy);
							px[0] = px[1] = 0;
							px[2] = 255;
						}
						ss += s.aim.sil[dy];
						for (int dx = 0; dx < ss; dx++)
						{
							uint8_t* px = s.bmp(xSkip - dx, ySkip + dy);
							px[0] = px[1] = 0;
							px[2] = 255;
						}
					}

					if (cnt)
					{
						sx /= cnt;
						sy /= cnt;
						for (int x = sx - 20; x <= sx + 20; x++)
							if (x >= 0 && x < s.bmp.w())
							{
								uint8_t* px = s.bmp(x, sy);
								px[0] = 0; px[1] = 255; px[2] = 0;
							}
						for (int y = sy - 20; y <= sy + 20; y++)
							if (y >= 0 && y < s.bmp.h())
							{
								uint8_t* px = s.bmp(sx, y);
								px[0] = 0; px[1] = 255; px[2] = 0;
							}
					}
				}

				if (s.found)
				{
					for (int x = s.aim.px - 10; x <= s.aim.px + 10; x++)
						if (x >= 0 && x < s.bmp.w())
						{
							uint8_t* px = s.bmp(x, s.aim.py);
							px[0] = 255; px[1] = 0; px[2] = 0;
						}
					for (int y = s.aim.py - 10; y <= s.aim.py + 10; y++)
						if (y >= 0 && y < s.bmp.h())
						{
							uint8_t* px = s.bmp(s.aim.px, y);
							px[0] = 255; px[1] = 0; px[2] = 0;
						}
				}
				s.bmp.Save((LPTSTR)path);
				s.bmp.Release();
				bmp.Release();
			}
		}
	}
}

void BotManager::GetStatusString(TCHAR* status)
{
	swprintf(status, L"%d", (int)GetFPS()); //ТУТ вылетает

	if (sDoSaveScreenshots)
		wcscat(status, L" [S]");

	BotManager::Mode mode = GetMode();
	static TCHAR* mode_str[] = { L"Off", L"Aim", L"Test" };
	if (mode >= 0 && mode < sizeof(mode_str) / sizeof(mode_str[0]))
	{
		wcscat(status, L" ");
		wcscat(status, mode_str[mode]);
	}

	if (WithoutLeftkey)
	{
		wcscat(status, L" DntH");
	}
	else
	{
		wcscat(status, L" HLft");
	}

	if (AimType == at_Color) wcscat(status, L" Clr");
	else if (AimType == at_Old) wcscat(status, L" Old");
	else if (AimType >= at_TrackerFirst && AimType <= at_TrackerLast) {
		wchar_t tt[20];
		swprintf(tt, L"Trk%d", (AimType - at_TrackerFirst));
		wcscat(status, tt);
	}
}
//===============================================================================
inline bool isRed(const uint8_t* pixel)
{
	/*
	BYTE minR = 150;//175
	BYTE maxR = 255;//255
	BYTE minG = 0;//0
	BYTE maxG = 75;//75
	BYTE minB = 0;//0
	BYTE maxB = 90;//50
	*/

	BYTE minR = 200;//175
	BYTE maxR = 255;//255
	BYTE minG = 0;//0
	BYTE maxG = 10;//75
	BYTE minB = 0;//0
	BYTE maxB = 30;//50

	if (pixel[2] >= minR && pixel[2] <= maxR &&
		pixel[1] >= minG && pixel[1] <= maxG &&
		pixel[0] >= minB && pixel[0] <= maxB)
	{
		return true;
	}
	else
		return false;
}
/*
inline bool isRedExtended(const uint8_t *pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[2] >= 200 && pixel[2] <= 255 &&
		pixel[1] >= 0 && pixel[1] <= 100 &&
		pixel[0] >= 4 && pixel[0] <= 100 ||
		pixel[2] >= 205 && pixel[2] <= 217 &&
		pixel[1] >= 0 && pixel[1] <= 7 &&
		pixel[0] >= 10 && pixel[0] <= 25)
		return true;

	return false;
}*/

inline bool isRedExtended(const uint8_t* pixel)
{
	if ((pixel[R] / ((float)pixel[G] + pixel[B] + 1.f)) > 0.9)
	{
		return true;
	}
	else
	{
		return false;
	}
}


inline bool isGreen(const uint8_t* pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[1] - 10 > pixel[2] && pixel[1] - 10 > pixel[0])
		return true;

	return false;
}

/*
inline bool isHealth(const uint8_t *pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[2] >= 250 && pixel[2] <= 255 &&
		pixel[1] >= 0 && pixel[1] <= 7 &&
		pixel[0] >= 4 && pixel[0] <= 20 ||
		pixel[2] >= 205 && pixel[2] <= 217 &&
		pixel[1] >= 0 && pixel[1] <= 7 &&
		pixel[0] >= 10 && pixel[0] <= 25)
		return true;

	return false;
}
*/
/*
inline bool isHealth(const uint8_t *pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[2] >= 220 && pixel[2] <= 255 &&
		pixel[1] >= 20 && pixel[1] <= 60 &&
		pixel[0] >= 40 && pixel[0] <= 90 ||
		pixel[2] >= 205 && pixel[2] <= 217 &&
		pixel[1] >= 0 && pixel[1] <= 7 &&
		pixel[0] >= 10 && pixel[0] <= 25)
		return true;

	return false;
}*/

/*
inline bool isHealth(const uint8_t* pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[2] >= 220 && pixel[2] <= 255 &&
		pixel[1] >= 20 && pixel[1] <= 110 &&
		pixel[0] >= 40 && pixel[0] <= 150 ||
		pixel[2] >= 205 && pixel[2] <= 217 &&
		pixel[1] >= 0 && pixel[1] <= 7 &&
		pixel[0] >= 10 && pixel[0] <= 25)
		return true;

	return false;
}*/

inline bool isHealth(const uint8_t* pixel)
{
	if ((pixel[R] / ((float)pixel[G] + pixel[B] + 1.f)) > 1.3)
	{
		return true;
	}
	else
	{
		return false;
	}
}

inline bool isBorder(const uint8_t* pixel) //pass by reference so to avoid re-allocating. Is probably still done by "return value optimization" but not sure.
{
	if (pixel[2] >= 170 && pixel[2] <= 255 &&
		pixel[1] >= 35 && pixel[1] <= 120 &&
		pixel[0] >= 20 && pixel[0] <= 110)
		return true;
	/*
	if (pixel[2] >= 225 && pixel[2] <= 255 &&
	pixel[1] >= 35 && pixel[1] <= 70 &&
	pixel[0] >= 20 && pixel[0] <= 60)
	*/
	return false;
}

inline void calibrateCoordinates(int& x, int& y, int w, int h)
{
	static const float constant = 0.090; // 0.116f; //0.090 seems to work better sometimes
	static float mouseSensitivity = 20.0f; //found in game menu
	static float modifier = mouseSensitivity * constant;

	//if (abs(x) < 5)
	//x = 0;
	//else {
	x = x - w / 2;
	//x = (int)((float)x / modifier);
	//x = (int)x;
	//}

	//if (abs(y) < 5)
	//	y = 0;
	//else
	//{
	y = y - h / 2;
	//y = (int)((float)y / modifier);
	//y = (int)y;
	//}
}

int calculateMedian(vector<int>& values)
{
	int median = 0;
	size_t size = values.size();

	sort(values.begin(), values.end());

	if (size % 2 == 0)
		median = (values[size / 2 - 1] + values[size / 2]) / 2;
	else
		median = values[size / 2];

	return median;
}

//===============================================================================
bool BotManager::TriggerBot(const Bitmap& bmp)
{
	/*
	static int cnt = 0;
	char ttt[256];
	sprintf(ttt, "FOUND (%d): %f \n", (++cnt) % 99, 1);
	::OutputDebugStringA(ttt);
	*/
	//срабатывание не на голову, блокирует возможност выстрела, стреляет сразу после открытия прицела

	static time_t sLastShotTime = 0;
	time_t timediff = timeGetTime() - sLastShotTime;
	if (timediff >= 1200)
	{
		int cx = bmp.w() / 2;
		int cy = bmp.h() / 2;
		int dx = 7;
		int dy = 7;
		int redCnt = 0;

		bool shoot = false;
		for (int y = cy - dy; y < cy + dy; y++)
		{
			for (int x = cx - dx; x < cx + dx; x++)
			{
				if (isRedExtended(bmp(x, y)) && !isRedExtended(bmp(x, y + 2)) && !isRedExtended(bmp(x, y - 2)))
				{
					redCnt++;
					//shoot = true;
					//break;
				}
			}
			//if (shoot) break;
		}

		if (redCnt > 0)
			shoot = true;

		if (shoot)
		{
			//mShot++;
			//mSave = kBMPToSave;
			//mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
			//Sleep(10);
			//mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			mouse.down();
			sLastShotTime = timeGetTime();
			Sleep(10);
			mouse.up();
			return true;
		}
	}

	return false;
}

//===============================================================================
//original ver find
bool FindPlayerOrigV(const Bitmap& bmp, int& posX, int& posY, bool headshot)
{
	int width = bmp.w();
	int height = bmp.h();
	int skipx = width / 4;
	int skipy = height / 4;
	bool foundHandle = false;

	//Scans from top -> down. Move right -> reset
	for (int x = skipx; x < width - skipx; x++)
		for (int y = skipy; y < height - skipy; y++)
		{
			if (isHealth(bmp(x, y))) //if pixel = health bar
			{
				//If adjacent pixels in the lower right direction are red => Hande of cluster Found
				if (x + 1 < width && y + 1 < height) //is NOT out of bounds
					if (isHealth(bmp(x + 1, y)) && isHealth(bmp(x, y + 1)))
						foundHandle = true;

				//If adjacent pixels in upper left are NOT red => Hande of cluster Found
				if (x - 1 >= 0 && y - 1 >= 0) //is NOT out of bounds
					if (isHealth(bmp(x - 1, y)) && isHealth(bmp(x, y - 1)))
						foundHandle = false; //THERE ARE REDS SO IT CAN'T BE HANDLE CORNER
			}

			int barWidth = 0;
			const int maxTol = 15;
			int tolerance = maxTol; //tolerance till we quit searching for red bars. Is reset if a red is found to maxTol
			if (foundHandle)
			{
				//Measure health bar width
				vector<int> barWidths;
				for (int _x = x; _x < width; _x++)
				{
					if (isRed(bmp(_x, y)))
					{
						tolerance = maxTol;
						barWidth++;
					}
					else
					{
						if (barWidth > 0)
							barWidths.push_back(barWidth);

						barWidth = 0;
						tolerance--;

						if (tolerance == 0)
							break;
					}
				}

				if (barWidths.size() == 0) //If no bars found, rekt shrekt and eeeerekt
					return false;

				int numberOfBars = 8; //IMP! FIX THIS FOR CHAMPS WITH MORE THAN 8 HEALTHBARS! (WINSTON, ROADHOG etc...)
				int healthBarWidth = 150;
				bool calculateBarWidth = true;

				if (barWidths.size() == 2) //1st bar should be accurate
					barWidth = barWidths[0];
				else if (barWidths.size() >= 3) //if we have 3 or more bars the median value should be accurate
					barWidth = calculateMedian(barWidths); //omits outliers
				else if (barWidths.size() <= 1) //we will assume that the healthBarWidth is 150; not a major problem because we will also scan an additional 1/3 healthBarWidth in -1 and +1 direction
					calculateBarWidth = false;

				healthBarWidth = calculateBarWidth ? (barWidth * numberOfBars) + ((numberOfBars - 1) * 2) : 150;

				//Obtain all border points lying under the health bar
				vector<POINT> border;

				int skipYs = 50; //use percent of screen height instead?

				bool foundAnyRedsBefore = false;
				bool foundRedInRow = false;
				int toleranceNoReds = (height * 5) / 100; //5% of image height

														  //Assign these to 0 before we add to them.
				posX = 0;
				posY = 0;

				for (int _y = y + skipYs; _y < height - skipy; y++) //scan down y
				{
					foundRedInRow = false;

					for (int _x = x - (healthBarWidth / 3); _x <= x + healthBarWidth + (healthBarWidth / 3); _x++) //scan in x
					{
						if (_x >= 0 && _x < width)
						{
							if (isRed(bmp(_x, _y)))
							{
								foundAnyRedsBefore = true;
								foundRedInRow = true;

								POINT pt;
								pt.x = _x;
								pt.y = _y;
								border.push_back(pt);

								posX += pt.x; //add all x's -> for avg
								posY += pt.y; //add all y's -> for avg
							}
						}
					}

					if (foundRedInRow == false)
						toleranceNoReds--;

					if (toleranceNoReds == 0)
						break;
				}
				//

				//if (foundAnyRedsBefore)
				//{
					//posX /= border.size();
					//if (!headshot)
					//	posY /= border.size();
					//else
					//{
					//	posX = border[0].x;// + (healthBarWidth / 4);
					//	posY = border[0].y + 10; //IMP +10 so we aim a little bit further down then the outline of the scalp head, this should be based on distance though!
					//}

					//return true;
				//}
				//else
				//{
					//return false;

				posX = x + (healthBarWidth / 2);
				posY = y + 65;

				return true;
				//}
			}
		}

	return false;
}

bool BotManager::FindHealth(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot)
{
	int width = bmp.w();
	int height = bmp.h();
	int xSkip = width / 4;
	int ySkip = height / 4;
	//int xSkip = width / 2-200;
	//int ySkip = height / 2-200;
	int xCenter = width / 2 - 50; //смещение
	int yCenter = height / 2 - 35;
	bool found = false;
	int Insum = 0;
	int Outsum = 0;

	HealthHeight = 6;
	posX = 0; posY = 0;

	/*
		for (int y = ySkip; y < height - ySkip; y++)
		{
			for (int x = xSkip; x < width - xSkip; x++)
			{
				Insum = 0;
				for (int h = 0; h < 5; h++)
				{
					for (int w = 0; w < 5; w++)
					{
						if(isHealth(bmp(x + w, y + h)))
							Insum++;
					}
				}
				Outsum = 0;
				for (int h = 0; h < 5; h++)
				{
					for (int w = 0; w < 5; w++)
					{
						if (isHealth(bmp(x - w, y - h)))
							Outsum++;
					}
				}

				if (Insum > 20 && Outsum < 6)
				//if (isHealth(bmp(x, y)) && isHealth(bmp(x+1, y)) &&
					//(isHealth(bmp(x, y + 1))) && (isHealth(bmp(x + 1, y + 1))) &&
					//!isHealth(bmp(x - 2, y)) && !(isHealth(bmp(x - 3, y))) && !(isHealth(bmp(x - 4, y))) && !(isHealth(bmp(x - 5, y))) )
				{
					if ( (sqrt((x - xCenter)*(x - xCenter) + (y - yCenter)*(y - yCenter))) <= (sqrt((posX - xCenter)*(posX - xCenter) + (posY - yCenter)*(posY - yCenter))) )
					{
							posX = x;
							posY = y;
							found = true;
					}
					//static int cnt = 0;
					//char ttt[256];
					//sprintf(ttt, "FOUND (%d): %d %d\n", (++cnt)%99, x, y);
					//::OutputDebugStringA(ttt);
				}
			}
		}
	*/

	for (int y = ySkip; y < height - ySkip; y++)
	{
		for (int x = xSkip; x < width - xSkip; x++)
		{
			if (isHealth(bmp(x, y)) && isHealth(bmp(x + 1, y)) &&
				(isHealth(bmp(x, y + 1))) && (isHealth(bmp(x + 1, y + 1))) &&
				!isHealth(bmp(x - 2, y)) && !(isHealth(bmp(x - 3, y))) && !(isHealth(bmp(x - 4, y))) && !(isHealth(bmp(x - 5, y))))
			{
				if ((sqrt((x - xCenter) * (x - xCenter) + (y - yCenter) * (y - yCenter))) <= (sqrt((posX - xCenter) * (posX - xCenter) + (posY - yCenter) * (posY - yCenter))))
				{
					posX = x;
					posY = y;
					found = true;
				}
				//static int cnt = 0;
				//char ttt[256];
				//sprintf(ttt, "FOUND (%d): %d %d\n", (++cnt)%99, x, y);
				//::OutputDebugStringA(ttt);
			}
		}
	}

	if (found)
	{
		//seek leftmost health pixel
		int pX = posX;
		for (int ix = 1; ix < 80; ix++)
			if (isRedExtended(bmp(pX - ix, posY)) && isRedExtended(bmp(pX - ix + 1, posY)) &&
				isRedExtended(bmp(pX - ix, posY + 1)) && isRedExtended(bmp(pX - ix + 1, posY + 1)) &&
				!isRedExtended(bmp(pX - ix - 2, posY)) && !isRedExtended(bmp(pX - ix - 3, posY)) && !isRedExtended(bmp(pX - ix - 4, posY)) && !isRedExtended(bmp(pX - ix - 5, posY)))
			{
				posX = pX - ix;
			}


		//seek bottom health pixel and measure height
		int rsum = 0;
		for (int iy = -8; iy < 12; iy++)
			if (isRedExtended(bmp(posX + 5, posY + iy))) //X+5 не работает с зарей
				rsum += 1;
			else
				if (rsum != 0)
				{
					posY = posY + iy - 1;
					break;
				}
		HealthHeight = rsum;


		/*
		static int cnt2 = 0;
		char ttt2[256];
		sprintf(ttt2, "FOUND (%d): %d \n", (++cnt2)%99, rsum);
		::OutputDebugStringA(ttt2);
		*/


		//TODO скачет из-за неправильного определения ширины хп
		HealthLen = 100;
		//int HealthLen = 0;
		if (bmp(posX + 117 + 3, posY - 2)[2] != 0)
		{
			float ColorRedRatioBG = (bmp(posX + 117 + 3, posY)[G] + bmp(posX + 117 + 3, posY)[B]) / bmp(posX + 117 + 3, posY)[R];
			for (int iX = 0; iX < 60; iX++)
			{
				if (bmp(posX + 117 - iX, posY)[R] != 0)
				{
					float ColorRedRatioM = (bmp(posX + 117 - iX, posY)[G] + bmp(posX + 117 - iX, posY)[B]) / bmp(posX + 117 - iX, posY)[R];
					if (ColorRedRatioBG - ColorRedRatioM > 0.25) //0.25
					{
						HealthLen = 117 - iX;
						break;
					}
					else
						ColorRedRatioBG = ColorRedRatioM;
				}

			}
		}

		if (HealthLen < 75)
			found = false;

		/*
		static int cnt = 0;
		char ttt[256];
		sprintf(ttt, "FOUND (%d): %d \n", (++cnt) % 99, HealthLen);
		::OutputDebugStringA(ttt);
		*/

		int rlen = 0;
		for (int ix = 0; ix < 15; ix++)
			if (isRedExtended(bmp(posX + ix, posY)))
				rlen += 1;
			else
				if (rlen != 0)
					break;
		HealthPartLen = rlen;

		/*
		static int cnt = 0;
		char ttt[256];
		sprintf(ttt, "FOUND (%d): %d \n", (++cnt) % 99, HealthPartLen);
		::OutputDebugStringA(ttt);
		*/

		//return true;
	}
	//else
		//return false;


	if (found)
	{
		return true;
	}
	else
		return false;

}

//find without hp bar
bool FindGreenPlayer(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot)
{
	posX = 0;
	posY = 0;
	int width = bmp.w();
	int height = bmp.h();
	int xSkip = width / 2 - 70;// width / 3;
	int ySkip = height / 2 - 70;// height / 3;
	int xCenter = width / 2; //смещение
	int yCenter = height / 2;
	int sumX = 0;
	int sumY = 0;
	int cntSum = 0;

	//TODO переделать на пиксели которые краснее чем соседние, а не просто красные
	for (int y = ySkip; y < height - ySkip; y++)
	{
		for (int x = xSkip; x < width - xSkip; x++)
		{
			/*
			if ( (isBorder(bmp(x, y)))   && (isBorder(bmp(x+1, y))) && (isBorder(bmp(x-1, y))) && (isBorder(bmp(x + 2, y))) && (isBorder(bmp(x - 2, y)))
				&& !(isBorder(bmp(x, y+1))) && !(isBorder(bmp(x, y-1))) && !(isBorder(bmp(x, y + 2))) && !(isBorder(bmp(x, y - 2))) && !(isBorder(bmp(x, y + 3))) && !(isBorder(bmp(x, y - 3)))
				&& !(isBorder(bmp(x+1, y + 1))) && !(isBorder(bmp(x+1, y - 1))) && !(isBorder(bmp(x+1, y + 2))) && !(isBorder(bmp(x+1, y - 2))) && !(isBorder(bmp(x+1, y + 3))) && !(isBorder(bmp(x+1, y - 3)))
				&& !(isBorder(bmp(x-1, y + 1))) && !(isBorder(bmp(x-1, y - 1))) && !(isBorder(bmp(x-1, y + 2))) && !(isBorder(bmp(x-1, y - 2))) && !(isBorder(bmp(x-1, y + 3))) && !(isBorder(bmp(x-1, y - 3))) ||
				 (isBorder(bmp(x, y)))   && (isBorder(bmp(x, y+1))) && (isBorder(bmp(x, y-1))) && (isBorder(bmp(x, y + 2))) && (isBorder(bmp(x, y - 2)))
				&& !(isBorder(bmp(x+1, y))) && !(isBorder(bmp(x-1, y))) && !(isBorder(bmp(x + 2, y))) && !(isBorder(bmp(x - 2, y))) && !(isBorder(bmp(x + 3, y))) && !(isBorder(bmp(x - 3, y)))
				&& !(isBorder(bmp(x + 1, y+1))) && !(isBorder(bmp(x - 1, y+1))) && !(isBorder(bmp(x + 2, y+1))) && !(isBorder(bmp(x - 2, y+1))) && !(isBorder(bmp(x + 3, y+1))) && !(isBorder(bmp(x - 3, y+1)))
				&& !(isBorder(bmp(x + 1, y-1))) && !(isBorder(bmp(x - 1, y-1))) && !(isBorder(bmp(x + 2, y-1))) && !(isBorder(bmp(x - 2, y-1))) && !(isBorder(bmp(x + 3, y-1))) && !(isBorder(bmp(x - 3, y-1))) )
			*/
			if (isGreen(bmp(x, y)) && isGreen(bmp(x + 1, y)) &&
				isGreen(bmp(x, y + 1)) && isGreen(bmp(x + 1, y + 1)))
			{
				//sumX = sumX + x;
				//sumY = sumY + y;
				//cntSum++;
				//if (cntSum == 10)
				//	break;
				//if (xCenter - posX > xCenter - x)
					//{
				posX = x;
				posY = y;
				//}
				return true;
			}
		}
		//if (cntSum == 10)
			//break;
	}


	//if (cntSum > 10)
	//{
		//posX = sumX / cntSum;
		//posY = sumY / cntSum;
	//}

	//posX = posX + 50; //смещение
	//posY = posY - 50;

	/*
	static int cnt = 0;
	char ttt[256];
	sprintf(ttt, "FOUND (%d): %d \n", (++cnt) % 99, posX);
	::OutputDebugStringA(ttt);
	*/

	//posX = 960;
	//posY = 600;
	//return true;

	if ((posX != 0) || (posY != 0))//-50)) //смещение
		return true;
	else
		return false;
}


//jump to red
//TODO сделать чтобы прыгал в ближайшую самую красную точку
bool BotManager::FindRedMax(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot, AimInfo& info)
{
	posX = 0;
	posY = 0;
	int width = bmp.w();
	int height = bmp.h();
	int xSkip = width / 2 - kWorkingAreaHalf;// width / 3;
	int ySkip = height / 2 - kWorkingAreaHalf;// height / 3;
	int xCenter = width / 2; //смещение
	int yCenter = height / 2;
	float maxRS = 0;
	float xSum = 0;
	float ySum = 0;
	float rCnt = 0;
	bool br = false;
	int hpbottom = -1;

	//int sil[kWorkingAreaHalf * 2];
	memset(info.sil, 0, sizeof(info.sil));
	memset(info.border, 0, sizeof(info.border));

	for (int y = ySkip; y < height - ySkip; y++)
	{
		for (int x = xSkip; x < width - xSkip; x++)
		{
			//по hp
			/*
			if (hpbottom < 0)
			{
				int rSum = 0;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 5; j++)
					{
						if (redStrength(bmp(x + j, y + i)) > 0.90f)
						{
							rSum++;
						}
						else
						{
							x += j;
							br = true;
							break;
						}
					}
					if (br)
					{
						br = false;
						break;
					}
				}

				if (rSum == 20)
				{
					//posX = x;
					//posY = y;
					//return true;
					hpbottom = y + 10;
				}
			}
	*/
	//типа по контуру
	//if (red > rMax && ((redXp > rMax && redXm > rMax && redYp <= rMax && redYm <= rMax) || (redXp <= rMax && redXm <= rMax && redYp > rMax && redYm > rMax)))
	//отличается от функции вывода сверху, плюс переменные на float переделаны
			float red = redStrength(bmp(x, y));
			float rMax = 1.0f;
			bool border = false;
			if (red > rMax)
			{
				int r1 = countRed(bmp, rMax, x, y, 1);
				if (r1 == 2 || r1 >= 3 && countRed(bmp, rMax, x, y, 2) == 2)
					border = true;
			}

			if (border)
			{
				if (red > 2) red = 2;
				rCnt += red;
				xSum += x * red;
				ySum += y * red;
				info.sil[y - ySkip] += red;
				info.border[x - xSkip][y - ySkip] = true;
			}

			/*//по точке
			if((red > maxRS + (y - ySkip) * 0.005)) //prefer topmost red
			{
				char ttt[256];
				sprintf(ttt, "RED: %f (%d) (%d)\n", red, x, y);
				::OutputDebugStringA(ttt);

				maxRS = red - (y - ySkip) * 0.005;
				posX = x;
				posY = y;
				//static std::vector<int> maxRSV;
				//maxRSV.push_back(25);
			}*/

		}
	}
	//return false;
	//типа по контуру
	if (rCnt != 0)
	{
		//conter of mass
		posX = xSum / rCnt;
		posY = ySum / rCnt;


		/*
		int rSumsil = 0;
		int headDel = -1;
		bool headEnd = false;
		int headEndPos = 0;
		int freeSum = 0;
		for (int dy = 0; dy < kWorkingAreaHalf * 2; dy++)
		{
			rSumsil += info.sil[dy];
			if (rSumsil > 5)
			{
				headEnd = true;
				headEndPos = dy;
				break;
			}
		}

		if (headEnd)
		{
			for (int dy = headEndPos; dy < headEndPos + 15; dy++)
			{
				if (!info.sil[dy])
				{
					freeSum++;
				}
				if (freeSum > 3)
				{
					headDel = dy;
					break;
				}
			}

		}*/

		enum { st_SkipEmpty, st_Text, st_Space, st_Player, st_Found, st_Failed } state = st_SkipEmpty;
		int sum = 0;
		int cntEmpty = 0;
		int textH = 0;
		int textEnd = -1;
		for (int dy = 0; dy < kWorkingAreaHalf * 2; dy++)
		{
			switch (state)
			{
			case st_SkipEmpty:
				sum += info.sil[dy];
				if (sum > 5) state = st_Text;
				break;
			case st_Text:
				if (!info.sil[dy]) cntEmpty++;
				textH++;
				if (cntEmpty > 1 || textH > 10)
				{
					if (textH >= 2)
					{
						textEnd = dy;
						cntEmpty = 0;
						state = st_Space;
					}
					else
						state = st_Failed;
				}
				break;
			case st_Space:
				if (!info.sil[dy]) cntEmpty++;
				else if (cntEmpty > textH && cntEmpty < textH * 5)
				{
					state = st_Player;
					sum = 0;
				}
				else state = st_Failed;
				break;
			case st_Player:
				sum += info.sil[dy];
				if (sum > rCnt * 7 / 10) state = st_Found;
				else if (dy == kWorkingAreaHalf * 2 - 1) state = st_Failed;
				break;
			}

			if (state == st_Found || state == st_Failed)
				break;
		}

		int scnt = rCnt;
		int dys = 0;

		if (state == st_Found)
		{
			dys = textEnd;
			for (int dy = 0; dy < textEnd; dy++)
				scnt -= info.sil[dy];
		}

		if (scnt > 0) //лимит, чтобы в режиме буз нажатия не работало на мусор
		{
			int headcnt = scnt / 4;
			int sils = 0;
			for (int dy = dys; dy < kWorkingAreaHalf * 2; dy++)
				if ((sils += info.sil[dy]) >= headcnt)
				{
					posY = dy + ySkip;
					break;
				}
			return true;
		}
		else
			return false;
	}
	else
	{
		return false;
	}

	//if ((posX != 0) || (posY != 0))
	/*//по точке
	if(maxRS > 0.60)
		return true;
	else
		return false;*/
}

bool BotManager::Track(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot, AimInfo& info)
{
	/*	posX = 0;
		posY = 0;
		int width = bmp.w();
		int height = bmp.h();
		int xSkip = width / 2 - kWorkingAreaHalf;// width / 3;
		int ySkip = height / 2 - kWorkingAreaHalf;// height / 3;
		int xCenter = width / 2; //смещение
		int yCenter = height / 2;
		float maxRS = 0;
		float xSum = 0;
		float ySum = 0;
		float rCnt = 0;
		bool br = false;
		int hpbottom = -1;

		//int sil[kWorkingAreaHalf * 2];
		memset(info.sil, 0, sizeof(info.sil));
		memset(info.border, 0, sizeof(info.border));

		Rect2d bbox(287, 23, 86, 320);

		tracker = TrackerBoosting::create();
		tracker->init(frame, bbox);
		*/
	return true;
}

std::string parse_python_exception() {
	PyObject* type_ptr = NULL, * value_ptr = NULL, * traceback_ptr = NULL;
	// Fetch the exception info from the Python C API
	PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);

	// Fallback error
	std::string ret("Unfetchable Python error");
	// If the fetch got a type pointer, parse the type into the exception string
	if (type_ptr != NULL) {
		py::handle<> h_type(type_ptr);
		py::str type_pstr(h_type);
		// Extract the string from the boost::python object
		py::extract<std::string> e_type_pstr(type_pstr);
		// If a valid string extraction is available, use it 
		//  otherwise use fallback
		if (e_type_pstr.check())
			ret = e_type_pstr();
		else
			ret = "Unknown exception type";
	}
	// Do the same for the exception value (the stringification of the exception)
	if (value_ptr != NULL) {
		py::handle<> h_val(value_ptr);
		py::str a(h_val);
		py::extract<std::string> returned(a);
		if (returned.check())
			ret += ": " + returned();
		else
			ret += std::string(": Unparseable Python error: ");
	}
	// Parse lines from the traceback using the Python traceback module
	if (traceback_ptr != NULL) {
		py::handle<> h_tb(traceback_ptr);
		// Load the traceback module and the format_tb function
		py::object tb(py::import("traceback"));
		py::object fmt_tb(tb.attr("format_tb"));
		// Call format_tb to get a list of traceback strings
		py::object tb_list(fmt_tb(h_tb));
		// Join the traceback strings into a single string
		py::object tb_str(py::str("\n").join(tb_list));
		// Extract the string, check the extraction, and fallback in necessary
		py::extract<std::string> returned(tb_str);
		if (returned.check())
			ret += ": " + returned();
		else
			ret += std::string(": Unparseable Python traceback");
	}
	return ret;
}

bool BotManager::AimBot(const Bitmap& bmp, AimInfo& info, AimTypeT at)
{
	//static time_t QLastTime = 0;
	//QLastTime = timeGetTime();
	int x, y, height, len, partLen;
	static float heightSum = 0;
	static int heightNum = 0;
	static float medianHeight;
	static int lenSum = 0;
	static int lenNum = 0;
	static int medianLen;



	{   
		try {
			static bool init_first = true;
			if (init_first) {
				Py_SetProgramName(L"test");  /* optional but recommended */
				Py_Initialize();
				np::initialize();

				module = new py::object(py::import("__main__"));
				name_space = new py::object(module->attr("__dict__"));


				/*
				struct gen_rand {
					double range;
				public:
					gen_rand(double r = 10.0) : range(r) {}
					double operator()() {
						return (rand() / (double)RAND_MAX) * range;
					}
				};

				int num_items = 1000;

				vector<double> v(num_items);
				generate_n(v.begin(), num_items, gen_rand());

				double sum_of_elems = 0;
				for (auto& n : v)
					sum_of_elems += n;
				cout << sum_of_elems << endl;

				int v_size = v.size();
				py::tuple shape1 = py::make_tuple(v_size);
				py::tuple stride1 = py::make_tuple(sizeof(double));
				np::dtype dt1 = np::dtype::get_builtin<double>();
				np::ndarray output = np::from_data(&v[0], dt1, shape1, stride1, py::object());
				output_array = new np::ndarray(output.copy());
				*/
				py::object res = exec_file("C:\\Users\\autoh\\source\\repos\\AutoAIMOver2\\script.py", *name_space, *name_space);


				init_first = false;
			}


		

			auto v = bmp.get_data();
			bmp.get_rowpitch();


			int v_size = bmp.h()*bmp.w()*4;
			py::tuple shape1 = py::make_tuple(v_size);
			py::tuple stride1 = py::make_tuple(sizeof(uint8_t));
			np::dtype dt1 = np::dtype::get_builtin<uint8_t>();
			np::ndarray output = np::from_data(v, dt1, shape1, stride1, py::object());

			py::object MainPyFunc = (*name_space)["MainPyFunc"];
			py::object result = MainPyFunc(output);

			auto sum2 = v[0] + v[1] + v[2] + v[3];
			double y = py::extract<int32_t>(result[0]);
			double x = py::extract<int32_t>(result[1]);
			double targetType = py::extract<int32_t>(result[2]);

			/*char ttt[256];
			sprintf(ttt, "(%f)\n", sum);
			::OutputDebugStringA(ttt);*/


			x /= 2;
			y /= 2;

			if (x || y)
			{
				mouse.move(x, y);
			}

		}
		catch (py::error_already_set& e)
		{
			std::string perror_str = parse_python_exception();
			std::cout << "Error in Python: " << perror_str << std::endl;
		}


		return 0;
	}









	if (at >= at_TrackerFirst && at <= at_TrackerLast)
	{
		int width0 = bmp.w();
		int height0 = bmp.h();
		int xCenter0 = width0 / 2;
		int yCenter0 = height0 / 2;

		int dx = width0 / 4;//edit here
		int dy = height0 / 4;//edit here

		int width = width0 - dx * 2;
		int height = height0 - dy * 2;
		int xCenter = width / 2;
		int yCenter = height / 2;

		Mat frame(height, width, CV_8UC4, (void*)(bmp.get_data() + dy * bmp.get_rowpitch() + 4 * dx), bmp.get_rowpitch());

		if (!tracker)
		{
			int objzone = 16;//edit here

			TrackerTypeT trackerType = (TrackerTypeT)(at - at_TrackerFirst);

			if (trackerType == tt_BOOSTING)
				tracker = TrackerBoosting::create();
			else if (trackerType == tt_MIL)
				tracker = TrackerMIL::create();
			else if (trackerType == tt_KCF)
				tracker = TrackerKCF::create();
			else if (trackerType == tt_TLD)
				tracker = TrackerTLD::create();
			else if (trackerType == tt_MEDIANFLOW)
				tracker = TrackerMedianFlow::create();
			else if (trackerType == tt_GOTURN)
				tracker = TrackerGOTURN::create();
			else if (trackerType == tt_MOSSE)
				tracker = TrackerMOSSE::create();
			else if (trackerType == tt_CSRT)
				tracker = TrackerCSRT::create();

			Rect2d bbox(xCenter - objzone, yCenter - objzone, objzone * 2, objzone * 2);
			try
			{
				tracker->init(frame, bbox);
			}
			catch (const cv::Exception& e) {
				::OutputDebugStringA(e.what());
			}
		}
		else
		{
			Rect2d bbox;
			bool ok;
			try
			{
				ok = tracker->update(frame, bbox);
			}
			catch (const cv::Exception& e) {
				::OutputDebugStringA(e.what());
			}
			if (ok)
			{
				x = bbox.x + bbox.width / 2 + dx;
				y = bbox.y + bbox.height / 2 + dy;
				//char ttt[256];
				//sprintf(ttt, "TRACKER: %d %d (%d %d %d %d)\n", x,y,(int)bbox.x, (int)bbox.y, (int)bbox.width, (int)bbox.height);
				//::OutputDebugStringA(ttt);

				info.px = x;
				info.py = y;

				calibrateCoordinates(x, y, bmp.w(), bmp.h());

				x /= 2;
				y /= 2;

				info.x = x; info.y = y;
				const int maxlim = 100;
				const int lim = 10;
				if (x > maxlim)		x = lim;
				else if (x < -maxlim)	x = -lim;
				if (y > maxlim)		y = lim;
				else if (y < -maxlim)	y = -lim;


				const int normlim = 40;
				if (x > normlim)		x = normlim;
				else if (x < -normlim)	x = -normlim;
				if (y > normlim)		y = normlim;
				else if (y < -normlim)	y = -normlim;

				const int minlim = 3;
				if (x < minlim && x > 0)	x = 0;
				else if (x > -minlim && x < 0)	x = 0;
				if (y < minlim && y > 0)		y = 0;
				else if (y > -minlim && y < 0)	y = 0;

				if (x || y)
				{
					mouse.move(x, y);
				}

			}
			return ok;
		}
		return true;
	}
	else if (at == at_Color)
	{
		if (FindRedMax(bmp, x, y, height, len, partLen, true, info))
		{
			info.px = x; info.py = y;

			calibrateCoordinates(x, y, bmp.w(), bmp.h());


			x /= 1.7;
			y /= 1.7;

			info.x = x; info.y = y;
			const int maxlim = 100;
			const int lim = 10;
			if (x > maxlim)		x = lim;
			else if (x < -maxlim)	x = -lim;
			if (y > maxlim)		y = lim;
			else if (y < -maxlim)	y = -lim;


			const int normlim = 40;
			if (x > normlim)		x = normlim;
			else if (x < -normlim)	x = -normlim;
			if (y > normlim)		y = normlim;
			else if (y < -normlim)	y = -normlim;

			const int minlim = 3;
			if (x < minlim && x > 0)	x = 0;
			else if (x > -minlim && x < 0)	x = 0;
			if (y < minlim && y > 0)		y = 0;
			else if (y > -minlim && y < 0)	y = 0;

			/*
			int dx = 0, dy = 0;
			if (lastPx >= 0) { dx = x - lastPx; dy = y - lastPy; }
			lastPx = x; lastPy = y;
			if (x > dx)
			{
				x += dx;
			}
			if (y > dy)
			{
				y += dy;
			}
			info.dx = dx; info.dy = dy;

			*/
			if (x || y)
			{
				mouse.move(x, y);
				//Sleep(300);
				/*
				Sleep(7);
				keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
				Sleep(1);
				keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);*/
			}
			return true;

		}

		lastPx = -1; lastPy = -1;
		return false;
	}


	/*
	if (FindGreenPlayer(bmp, x, y, height, len, partLen, true))
	{
		info.px = x; info.py = y;

		calibrateCoordinates(x, y, bmp.w(), bmp.h());*/
		/*
		x /= 2;
		y /= 2;

		const int maxlim = 40;
		const int lim = 5;
		if (x > maxlim)		x = lim;
		else if (x < -maxlim)	x = -lim;
		if (y > maxlim)		y = lim;
		else if (y < -maxlim)	y = -lim;


		const int normlim = 25;
		if (x > normlim)		x = normlim;
		else if (x < -normlim)	x = -normlim;
		if (y > normlim)		y = normlim;
		else if (y < -normlim)	y = -noddrmlim;
		*//*
		info.x = x; info.y = y;
		if (x || y)
		{
			mouse.move(x, y+5);
			keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
			Sleep(20);
			keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
			//mouse.down();
			//Sleep(20);
			//mouse.up();
			Sleep(120);
		}
		return true;
	}

	keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
	Sleep(5);
	keybd_event(VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	//Sleep(120);

	heightSum = 6.3;
	heightNum = 1;
	medianHeight = heightSum / heightNum;

	lastPx = -1; lastPy = -1;
	return false;
}*/
	else //if (at==at_Old)
	{
		if (FindHealth(bmp, x, y, height, len, partLen, true))
		{
			info.px = x; info.py = y;
			/*
			//предсказание движения
			int dx = 0, dy = 0;
			if (lastPx >= 0) { dx = x - lastPx; dy = y - lastPy; }
			lastPx = x; lastPy = y;
			x += dx; y += dy;
			info.dx = dx; info.dy = dy;
			*/
			//static time_t timediff = timeGetTime() - QLastTime;


			heightNum++;
			heightSum += height;

			if (heightNum > 10) //усреднение значений
			{
				medianHeight = heightSum / heightNum;
				heightNum = 0;
				heightSum = 0;
			}

			if (medianHeight == 0) //если не инициализировано
				medianHeight = height;



			lenNum++;
			lenSum += len;

			if (lenNum > 10) //усреднение значений
			{
				medianLen = lenSum / lenNum;
				lenNum = 0;
				lenSum = 0;
			}

			if (medianLen == 0) //если не инициализировано
				medianLen = len;

			/*
			static int cnt = 0;
			char ttt[256];
			sprintf(ttt, "FOUND (%d): %d %F %d \n", (++cnt) % 99, medianLen, medianHeight, partLen);
			::OutputDebugStringA(ttt);
			*/

			//TODO на зарю попрака для танков плохо работает
			if (medianHeight > 4) //поправка от длины хп, если не может посчитать высоту (<4), считает максимальнро далеко
			{
				x += medianLen / 2;
				//y += medianLen / 2 + 5;
				y += (medianLen - 82) * 1.2 + 20;
				if (partLen < 7) // поправка для танков
					y += (medianLen - 80) / 1.3;
			}
			else
			{
				x += 42; //смещение тела от здоровья
				y += 23;
			}



			/*
			if (medianHeight > 7)
			{
				x += 57; //смещение тела от здоровья
				y += 65;
			}
			else if (medianHeight > 6.7)
			{
				x += 55; //смещение тела от здоровья
				y += 62;
			}
			else if (medianHeight > 6.2)
			{
				x += 52; //смещение тела от здоровья
				y += 54;
			}
			else if (medianHeight > 5.8)
			{
				x += 46; //смещение тела от здоровья
				y += 48;
			}
			else if (medianHeight > 4.8)
			{
				x += 42; //смещение тела от здоровья
				y += 45;
			}
			else if (medianHeight > 1)
			{
				x += 42; //смещение тела от здоровья
				y += 45;
			}
			else if (medianHeight <= 1)
			{
				x += 50; //смещение тела от здоровья
				y += 55;
			}

			*/

			/*
			static int cnt = 0;
			char ttt[256];
			sprintf(ttt, "FOUND (%d): %f \n", (++cnt) % 99, medianHeight);
			::OutputDebugStringA(ttt);
			*/

			//x += 50; //смещение тела от здоровья
			//y += 60;



			calibrateCoordinates(x, y, bmp.w(), bmp.h());

			//if (sAimSpeed == 1)
			//x /= 4;
			//y /= 4;
			//x /= 2.5;
			//y /= 2.5;
			x /= 2;
			y /= 2;
			//x /= 1.7;
			//y /= 1.7;
			//x /= 1.5;
			//y /= 1.5;
			//x /= 1.3;
			//y /= 1.3;

			/*
			static int cnt = 0;
			char ttt[256];
			sprintf(ttt, "FOUND (%d): %d  %d \n", (++cnt) % 99, x, y);
			::OutputDebugStringA(ttt);
			*/
			/*
			const int minlim = 1;
			if ( ((x < minlim) && (x > 0)) || ((x > -minlim) && (x < 0)) )
			{
				x = x/1.5;
			}

			if ( ((y < minlim) && (y > 0)) || ((y > -minlim) && (y < 0)) )
			{
				y = y/1.5;
			}
			*/


			//const int maxlim1 = 20;
			//const int maxlim2 = 40;
			/*
			if ((x > maxlim1) || (x < -maxlim1) || (y > maxlim1) || (y < -maxlim1))
			{
				x = x/5;
				y = y/5;
			}

			else
				if ((x > maxlim2) || (x < -maxlim2) || (y > maxlim2) || (y < -maxlim2))
				{
					x = x / 2;
					y = y / 2;
				}
			*/

			/*
			const int maxlim1 = 50;
			if ((x > maxlim1) || (x < -maxlim1) || (y > maxlim1) || (y < -maxlim1))
			{
				x = 0;
				y = 0;
			}
			*/

			const int maxlim = 100;
			const int lim = 10;
			if (x > maxlim)		x = lim;
			else if (x < -maxlim)	x = -lim;
			if (y > maxlim)		y = lim;
			else if (y < -maxlim)	y = -lim;


			const int normlim = 40;
			if (x > normlim)		x = normlim;
			else if (x < -normlim)	x = -normlim;
			if (y > normlim)		y = normlim;
			else if (y < -normlim)	y = -normlim;


			/*
					if (x > 6)
						x = rand() % 6 + 4;
					if (x < -6)
						x = -(rand() % 6 + 4);
					if (y > 6)
						y = rand() % 6 + 4;
					if (y < -6)
						y = -(rand() % 6 + 4);
			*/
			info.x = x; info.y = y;
			if (x || y)
			{
				/*			char ttt[256];
							sprintf(ttt,"MOVE: %d %d\n", x, y);
							::OutputDebugStringA(ttt);*/
				mouse.move(x, y);
				//Sleep(120);
			}
			//Sleep(220);
			return true;
			//Sleep(5);
		}

		heightSum = 6.3;
		heightNum = 1;
		medianHeight = heightSum / heightNum;

		lastPx = -1; lastPy = -1;
		return false;
	}
}

//=========================================================
bool BotManager::TestMouse(int& dx)
{
	/*
	int x = 10;
	int y = 10;
	mouse.move(x, y);
	return false;
	*/

	static DWORD time = 0;
	DWORD newtime = timeGetTime();
	int dlt = newtime - time;
	if (dlt > 100) dlt = 100;

	if (time != 0)
	{
		dx = time != 0 ? (dlt * 5 * 60 + 500) / 1000 : 5;
		mouse.move(dx, 0);
	}

	time = newtime;

	return false;
}

//===============================================================================
void BotManager::DisplayCallback(ID3D11Texture2D* d3dtex, ID3D11Device* pDevice)
{
	time_t ct = timeGetTime();
	if (sLastCBCounter == 0)
	{
		sLastCBTime = ct;
		sLastCBCounter = 1;
	}
	if (ct - sLastCBTime > 1000 && sLastCBCounter > 10)
	{
		sFPS = (int)((sLastCBCounter * 1000) / (ct - sLastCBTime));
		//fprintf(stdout, "FPS: %d", sFPS);

		sLastCBTime = ct;
		sLastCBCounter = 1;
	}
	else
		sLastCBCounter++;

	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc;
	ID3D11Texture2D* pNewTexture = NULL;

	D3D11_TEXTURE2D_DESC description;

	d3dtex->GetDesc(&desc);
	d3dtex->GetDesc(&description);

	description.BindFlags = 0;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	description.Usage = D3D11_USAGE_STAGING;
	description.MiscFlags = 0;

	if (FAILED(pDevice->CreateTexture2D(&description, NULL, &pNewTexture))) return;

	ID3D11DeviceContext* ctx = NULL;
	pDevice->GetImmediateContext(&ctx);

	ctx->CopyResource(pNewTexture, d3dtex);

	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	ctx->Map(pNewTexture, subresource, D3D11_MAP_READ_WRITE, 0, &resource);

	Bitmap bmp(reinterpret_cast<uint8_t*>(resource.pData), desc.Width, desc.Height, resource.RowPitch);

	Process(bmp);

	pNewTexture->Release();
}

