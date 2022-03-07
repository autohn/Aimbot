#pragma once

#include "CommonTypes.h"
#include "Mouse.h"
#include "Bitmap.h"

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

#define BOOST_PYTHON_STATIC_LIB
#define BOOST_NUMPY_STATIC_LIB
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

namespace py = boost::python;
namespace np = boost::python::numpy;

class BotManager
{
public:
	BotManager();
	~BotManager() { Py_Finalize(); }

	static void DisplayCallback(void* obj, ID3D11Texture2D* d3dtex, ID3D11Device* pDevice) { ((BotManager*)obj)->DisplayCallback(d3dtex, pDevice); }

	int GetFPS() const { return sFPS; }
	enum Mode { md_Off, md_Aim, md_TestMouse };
	Mode GetMode() const { if (sDoTestMouse) return md_TestMouse; if (sDoAimBot) return md_Aim; else return md_Off; }
	void GetStatusString(TCHAR* status);


private:
	void DisplayCallback(ID3D11Texture2D* d3dtex, ID3D11Device* pDevice);
	void Process(Bitmap &bmp);

	bool TriggerBot(const Bitmap &bmp);

	enum { kWorkingAreaHalf = 100 };
	struct AimInfo 
	{
	AimInfo() { memset(this, 0, sizeof(AimInfo)); }
	int px, py;		//body screen position
	int x, y;		//mouse relative movement
	int dx, dy;		//movement prediction
	float sil[kWorkingAreaHalf*2];
	bool border[kWorkingAreaHalf * 2][kWorkingAreaHalf * 2];
	};
	bool FindHealth(const Bitmap &bmp, int &posX, int &posY, int &HealthHeight, int &HealthLen, int &HealthPartLen, bool headshot);
	typedef enum{ tt_BOOSTING, tt_MIL, tt_KCF, tt_TLD, tt_MEDIANFLOW, tt_GOTURN, tt_MOSSE, tt_CSRT, tt_Last} TrackerTypeT;
	typedef enum { at_Old, at_Color, at_TrackerFirst, at_TrackerLast= at_TrackerFirst+tt_Last-1, at_Last } AimTypeT;
	bool AimBot(const Bitmap &bmp, AimInfo &info, AimTypeT at);
	bool FindRedMax(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot, AimInfo& info);

	bool Track(const Bitmap& bmp, int& posX, int& posY, int& HealthHeight, int& HealthLen, int& HealthPartLen, bool headshot, AimInfo& info);
	cv::Ptr<cv::Tracker> tracker;

	bool TestMouse(int &dx);

private:
	Mouse mouse;

	time_t sLastCBTime;
	int sLastCBCounter;
	int sFPS;
	int sMode;

	bool sCapsLockState, sDoAimBot;
	bool sKey1State, sDoTestMouse;
	bool sKey2State, sDoSaveScreenshots;
	bool sKey3State, WithoutLeftkey;
	bool sKey4State;
	AimTypeT AimType;
	int sAimSpeed;

	int lastPx, lastPy;

private:
	enum { kBMPToSave = 60 };
	struct Screenshot {
		Bitmap bmp;
		time_t time;
		AimInfo aim;
		bool found;
	} mScr[kBMPToSave];
	int mSave;
	int mShot;

private:
	py::object *module;
	py::object *name_space;
	np::ndarray *output_array;

};

