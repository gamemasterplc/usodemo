#include <pad.h>
#include <debug.h>
#include "zoom_camera.h"

//Camera constants
const float MOVE_SPEED = 0.03f;
const int MOVE_DEADZONE = 16;
const float ZOOM_SPEED = 0.01f;
const float MIN_ZOOM = 0.25f;
const float MAX_ZOOM = 4.0f;

ZoomCamera::ZoomCamera()
{
	//Initialize camera settings
	m_x = m_y = 0;
	m_zoom = 1.0f;
	debug_printf("ZoomCamera constructor called.\n");
}

ZoomCamera::~ZoomCamera()
{
	debug_printf("ZoomCamera destructor called.\n");
}

float ZoomCamera::GetX()
{
	return m_x;
}

float ZoomCamera::GetY()
{
	return m_y;
}

float ZoomCamera::GetZoom()
{
	return m_zoom;
}

void ZoomCamera::Update()
{
	u16 held = PadGetHeldButtons(0);
	s8 stick_x = PadGetStickX(0);
	s8 stick_y = PadGetStickY(0);
	//Calculate zoom change
	float zoom_delta = 0;
	if(held & L_TRIG) {
		//Zoom out with L
		zoom_delta = -MOVE_SPEED*m_zoom;
	}
	if(held & R_TRIG) {
		//Zoom in with R
		zoom_delta = MOVE_SPEED*m_zoom;
	}
	//Change zoom
	m_zoom += zoom_delta;
	//Clamp zoom
	if(m_zoom < MIN_ZOOM) {
		m_zoom = MIN_ZOOM;
	}
	if(m_zoom > MAX_ZOOM) {
		m_zoom = MAX_ZOOM;
	}
	//Move camera with stick
	if(stick_x > MOVE_DEADZONE || stick_x < -MOVE_DEADZONE) {
		m_x += stick_x*MOVE_SPEED;
	}
	if(stick_y > MOVE_DEADZONE || stick_y < -MOVE_DEADZONE) {
		m_y -= stick_y*MOVE_SPEED;
	}
}