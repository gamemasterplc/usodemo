#pragma once

class ScrollCamera {
	public:
	//Constructors/destructors
		ScrollCamera();
		~ScrollCamera();
		
	public:
	//Getters
		float GetX();
		float GetY();
		void Update(); //Update camera based on input
		
	private:
		float m_x;
		float m_y;
};