#include <iostream>
#include "point.h"
#include <windows.h>

#define width 120
#define height 30
int main()
{
	listManager<Vector2D> pts;
	listPiece<Vector2D>* start = pts.getCurrentListPiece();

	Vector2D pos;
	pos.x = 0;
	pos.y = 0;

	

	double x = 0;
	double y = 0;
	double _time = 0;
	while (true)
	{
		std::cout << "x: ";
		std::cin >> x;
		if (x == 21)
			goto interp;
		std::cout << "y: ";
		std::cin >> y;
		std::cout << "time: ";
		std::cin >> _time;

		pts.insertPoint(Vector2D(x, y), _time);

		listPiece<Vector2D>* current = start;
		do
		{
			std::cout << "< " << current->time << ", < " << current->value.x << ", " << current->value.y << " > >\n";
			current = current->getNext();
		} while (current);
	}
interp:
#if 0
	while (true)
	{
		std::cout << "Time of point to get: ";
		std::cin >> time;
		Vector2D gotValue = pts.getPoint(time);
		std::cout << "Got: " << gotValue.x << ", " << gotValue.y << '\n';
	}
#endif
	LARGE_INTEGER perTime;
	LARGE_INTEGER timePerSecond;
	LARGE_INTEGER startTime;
	QueryPerformanceCounter(&startTime);
	QueryPerformanceFrequency(&timePerSecond);

	char field[width * height] = {};
	for (size_t i = 0; i < (width * height - 1); ++i)
	{
		field[i] = ' ';
	}
	while (true)
	{
		QueryPerformanceCounter(&perTime);
		double time = (double)(perTime.QuadPart - startTime.QuadPart) / timePerSecond.QuadPart;

		Vector2D wantedPos = pts.getPoint(time);
		pos += (wantedPos - pos) * (wantedPos - pos); // squared relationship

		int index = (((int)pos.x) + ((int)pos.y * width));
		if (index < width * height && index >= 0)
		{
			field[index] = '@';
		}
		std::cout << '\n';
		std::cout << field << std::flush;
		if (index < width * height && index >= 0)
		{
			field[((int)pos.x) + ((int)pos.y * width)] = ' ';
		}
		
	}

}
