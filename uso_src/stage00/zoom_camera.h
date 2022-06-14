#pragma once

class ZoomCamera {
	public:
	//Constructors/destructors
		ZoomCamera();
		~ZoomCamera();
		
	public:
	//Getters
		float GetX();
		float GetY();
		float GetZoom();
		void Update(); //Update camera based on input
		
	private:
		float m_x;
		float m_y;
		float m_zoom;
};