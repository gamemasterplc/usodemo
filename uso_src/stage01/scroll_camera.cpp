#include <pad.h>
#include <debug.h>
#include "scroll_camera.h"

const float MOVE_SPEED = 0.03f;
const int MOVE_DEADZONE = 16;

ScrollCamera::ScrollCamera()
{
	m_x = m_y = 0;
	debug_printf("ScrollCamera constructor called.\n");
}

ScrollCamera::~ScrollCamera()
{
	debug_printf("ScrollCamera destructor called.\n");
}

float ScrollCamera::GetX()
{
	return m_x;
}

float ScrollCamera::GetY()
{
	return m_y;
}

void ScrollCamera::Update()
{
	s8 stick_x = PadGetStickX(0);
	s8 stick_y = PadGetStickY(0);
	//
	if(stick_x > MOVE_DEADZONE || stick_x < -MOVE_DEADZONE) {
		m_x += stick_x*MOVE_SPEED;
	}
	if(stick_y > MOVE_DEADZONE || stick_y < -MOVE_DEADZONE) {
		m_y -= stick_y*MOVE_SPEED;
	}
}