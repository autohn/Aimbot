#include "Mouse.h"
#include <interception.h>
#include <windows.h>

int Mouse::counter = 0;

Mouse::Mouse()
{
	 Init();
}

Mouse::~Mouse()
{
	DeInit();
}

static InterceptionContext context;
static InterceptionDevice device;

void Mouse::Init()
{
	if (counter++ > 0) return;

	context = interception_create_context();
	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE);
	InterceptionStroke stroke;
	bool inited = false;
	while (interception_receive(context, device = interception_wait(context), &stroke, 1) > 0)
	{
		if (interception_is_mouse(device))
		{
			InterceptionMouseStroke &mstroke = *(InterceptionMouseStroke *)&stroke;
			interception_send(context, device, &stroke, 1);
			inited = true;
			break;
		}

	}
	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_NONE);
	if (!inited)
		::MessageBox(0, L"Error! Mouse Interceptor failed to initialize!", L"BT2 Initialization", MB_OK | MB_ICONSTOP);
}

void Mouse::DeInit()
{
	if (--counter > 0) return;
	interception_destroy_context(context);
}

void Mouse::move(int x, int y)
{
	InterceptionMouseStroke mstroke;
	mstroke.state = 0;
	mstroke.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;
	mstroke.rolling = 0;
	mstroke.x = x;
	mstroke.y = y;
	mstroke.information = 0;

	interception_send(context, device, (InterceptionStroke*)&mstroke, 1);
}

void Mouse::down()
{
	InterceptionMouseStroke mstroke;
	mstroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN;
	mstroke.flags = 0;
	mstroke.rolling = 0;
	mstroke.x = 0;
	mstroke.y = 0;
	mstroke.information = 0;
	interception_send(context, device, (InterceptionStroke*)&mstroke, 1);
}

void Mouse::up()
{
	InterceptionMouseStroke mstroke;
	mstroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
	mstroke.flags = 0;
	mstroke.rolling = 0;
	mstroke.x = 0;
	mstroke.y = 0;
	mstroke.information = 0;
	interception_send(context, device, (InterceptionStroke*)&mstroke, 1);
}
