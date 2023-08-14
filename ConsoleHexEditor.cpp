// ConsoleHexEditor.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <conio.h>
#include <windows.h>

#define width (size_t)120
#define height (size_t)30

#define DWORDExpand(d) ((size_t)(unsigned int)d)

#define scrollAmount width * 3
#define bufferSize (height - 2) * (width / 4)

HANDLE hFile = NULL;
PWSTR inStr = NULL;
DWORD bytesWritten = 0;
BYTE fileBytesDisplaying[bufferSize] = {};
DWORD bytesRead = 0;
OVERLAPPED ovlpt = {};
OVERLAPPED writeOvlpt = {};
wchar_t typedInput = NULL;
BOOL bResult;
DWORD dwError;
LARGE_INTEGER fileSize;

size_t numGroupedData = 0;

// functions
void readValues();
void drawValues();
void drawCursor();
void drawInfo();

wchar_t* getScreenClearBuffer()
{
	size_t sz = width * height; // normaly would be this + 1, but I want it do display 1 character less so the screen does not scroll to the next line
	wchar_t* newScreenClearBuffer = new wchar_t[sz];
	for (size_t i=0;i<sz - 1;++i)
	{
		newScreenClearBuffer[i] = ' ';
	}
	newScreenClearBuffer[sz - 1] = '\0';
	
	return newScreenClearBuffer;
}


size_t wstrlen(wchar_t* str)
{
	size_t len = 0;
	while (str[len] != '\0')
		++len;
	return len;
}

HANDLE hStdInput = GetStdHandle(STD_INPUT_HANDLE);
HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);


class CursorPos
{
public:
	COORD coord = {};

	operator COORD ()
	{
		return coord;
	}
	CursorPos& operator=(COORD crd)
	{
		coord = crd;
		SetConsoleCursorPosition(hStdOutput, crd);
		return *this;
	}
	CursorPos& operator+=(COORD crd)
	{

		coord.X += crd.X;
		coord.Y += crd.Y;
		if (coord.X > width)
		{
			++coord.Y;
			coord.X %= width;
		}
		SetConsoleCursorPosition(hStdOutput, coord);
		return *this;
	}
	CursorPos& operator-=(COORD crd)
	{
		coord.X -= crd.X;
		coord.Y -= crd.Y;
		if (coord.X < 0)
		{
			coord.Y += -1 + (coord.X + 1) / width;
			coord.X %= width;
		}
		SetConsoleCursorPosition(hStdOutput, coord);
		return *this;
	}
};

class myConsoleHandler
{
	wchar_t* screenClearBuffer = getScreenClearBuffer(); // maybe it should be initialized to 0 first (maybe)
	wchar_t* screenBuffer = new wchar_t[width * height + 1];
public:
	CursorPos cursorPos = {};

	myConsoleHandler& operator<<(wchar_t *input)
	{
		size_t len = wstrlen(input);
		cursorPos.coord.X += len;
		if (cursorPos.coord.X > width)
		{
			++cursorPos.coord.Y;
			cursorPos.coord.X %= len + 1;
		}
		std::wcout << input;
		return *this;
	}
	myConsoleHandler& operator<<(const wchar_t* input)
	{
		size_t len = wstrlen((wchar_t*)input);
		cursorPos.coord.X += len;
		if (cursorPos.coord.X > width)
		{
			++cursorPos.coord.Y;
			cursorPos.coord.X %= len + 1;
		}
		std::wcout << input;
		return *this;
	}
	myConsoleHandler& operator<<(wchar_t input)
	{
		size_t len = 1;  // a single character (please don't put tabs or enters in here)
		cursorPos.coord.X += len; // Update the cursor pos (without having it also set it)
		if (cursorPos.coord.X > width)
		{
			++cursorPos.coord.Y;
			cursorPos.coord.X %= len + 1;
		}
		std::wcout << (short)input;
		return *this;
	}
	myConsoleHandler& operator<<(BYTE input)
	{
		size_t len = 3;  // always looks like $00 so 3 is always the len (hex)
		cursorPos.coord.X += len; // Update the cursor pos (without having it also set it)
		if (cursorPos.coord.X > width)
		{
			++cursorPos.coord.Y; // Does not currently support strings that go beyond the width * 2
			cursorPos.coord.X %= len + 1;
		}
		
		std::wcout << '$';
		if (input <= 0xf) // add a leading 0 if it is not 2 chars long
			std::wcout << '0';
		std::wcout << (short)input;

		if(numGroupedData)
		{
			LARGE_INTEGER li{ovlpt.Offset,ovlpt.OffsetHigh};
			if (((li.QuadPart + (cursorPos.coord.X + (cursorPos.coord.Y * width)) + 1) % numGroupedData) == 0)
			{
				operator<<(L"   "); // three characters to keep the values from being cut off at the end when displayed (not sure this comment is clear enough)
			}
		}
		return *this;
	}

	void clearScreen()
	{
		// Right now, it's expected for the cursor to be at (0, 0) for this to work properly
		// Slow method for clearing the screan, but it works. (and I'm too lazy to make it faster)
		
		std::wcout << screenClearBuffer;
		std::wcout << std::flush;
		SetConsoleCursorPosition(hStdOutput, cursorPos.coord);   // Change to string and add null for diff sizes
	}
	
	void clearRow()
	{
		// Cursor needs to be at the start of the row
		size_t nullPosition = width;
		screenClearBuffer[nullPosition] = 0;
		std::wcout << screenClearBuffer;
		std::wcout << std::flush;
		screenClearBuffer[nullPosition] = ' ';
		SetConsoleCursorPosition(hStdOutput, cursorPos.coord);
	}
	void clearChars(size_t chars)
	{
		if (!chars)
			return;
		size_t nullPosition = chars;
		screenClearBuffer[nullPosition] = 0;
		std::wcout << screenClearBuffer;
		std::wcout << std::flush;
		screenClearBuffer[nullPosition] = ' ';
		SetConsoleCursorPosition(hStdOutput, cursorPos.coord);
	}

	~myConsoleHandler()
	{
		delete[] screenClearBuffer;
		delete[] screenBuffer;
	}
} consoleHandler;

class selectCursor
{
public:
	int pos = 0;
	selectCursor& operator=(int i)
	{

		COORD oldCursorPos = consoleHandler.cursorPos.coord;

		consoleHandler.cursorPos = COORD{ ((short)pos % ((short)width / 4)) * 4, (short)pos / ((short)width / 4) + 1 };
		consoleHandler << L" ";
		consoleHandler.cursorPos += COORD{ 2, 0 };
		consoleHandler << L" ";

		consoleHandler.cursorPos = oldCursorPos;
		pos = i;
		return *this;
	}

	selectCursor& operator++()
	{
		this->operator=(pos + 1);
		return *this;
	}

	selectCursor& operator--()
	{
		this->operator=(pos - 1);
		return *this;
	}

	operator int()
	{
		return pos;
	}
} cursorPos;

bool validFilepath(wchar_t* str)
{
	const wchar_t invalidPathChars[] = L"<>|\"*";
	const wchar_t invalidFileNameChars[] = L"\\/?:"; // the invalidPathChars are also invalidFileNameChars
	// Basic check for now (I don't feel like making sure this is robust enough)
	size_t startOfFileName = 0;
	for (size_t i = 0; str[i] != '\0'; ++i)
	{
		for (size_t j = 0; j < sizeof(invalidPathChars) - 1; ++j)
		{
			if (str[i] == invalidPathChars[j])
				return false;
		}
		if (str[i] == '\\' || str[i] == '/')
			startOfFileName = i + 1;
	}
	for (size_t i = startOfFileName; str[i] != '\0'; ++i)
	{
		for (size_t j = 0; j < sizeof(invalidFileNameChars) - 1; ++j)
		{
			if (str[i] == invalidFileNameChars[j])
				return false;
		}
	}
	return wstrlen(str) > 0;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc >= 2)
	{
		if (validFilepath(argv[1]))
		{
			hFile = CreateFileW(argv[1], GENERIC_READ | GENERIC_WRITE, NULL, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
			/*
			for (size_t i = 2; i < argc; ++i)
				WriteFile(hFile, argv[i], wstrlen(argv[i]), &bytesWritten, NULL);
				*/
		}
		else
			std::wcout << "Invalid Path" << std::endl;
	}


	std::wcout << std::hex << std::uppercase;
	consoleHandler.cursorPos = COORD{ 0, 0 };
	consoleHandler.clearScreen();

	consoleHandler << L"My Hex Editor!\n";
	while (!hFile)
	{
		consoleHandler << L"Enter a file path to edit: ";
		wchar_t filePath[261] = {}; // The normal file path limit is 260. It can be extended, but I don't feel like supporting it
		std::wcin >> filePath;
		if (validFilepath(filePath))
		{
			hFile = CreateFileW(filePath, GENERIC_READ | GENERIC_WRITE, NULL, NULL, OPEN_ALWAYS, NULL, NULL);
		}
		else
		{
			consoleHandler.cursorPos = COORD{ 0, 1 };
			consoleHandler.clearChars((height - 1) * width - 1);
			consoleHandler << L"Invalid Path\n";
			continue;
		}
		consoleHandler.cursorPos = COORD{ 0, 1 };
		consoleHandler.clearChars((height - 1) * width - 1);
	}


	
	ovlpt.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ovlpt.hEvent == NULL)
	{
		return 1;
	}
	
	
	GetFileSizeEx(hFile, &fileSize);

	consoleHandler.cursorPos = COORD{ 0, 1 };
	consoleHandler.clearChars((height - 1) * width - 1);
	readValues();
	drawValues();
	drawCursor();
	drawInfo();
	
	
	writeOvlpt.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	while (true)
	{
		typedInput = _getch();
		switch (typedInput)
		{
		case 'g':
		{
			// group bytes by typed number
			size_t input;
			consoleHandler.cursorPos = COORD{ 0, height - 1 };
			consoleHandler.clearChars(width - 1);
			std::cin >> input;
			numGroupedData = input;
			consoleHandler.cursorPos = COORD{ 0, 0 };
			consoleHandler.cursorPos = COORD{ 0, 1 };
			cursorPos = 0;
			consoleHandler.clearScreen();
			drawValues();
			drawCursor();
		}
		break;
		case 'j':
		{
			// Jump to typed position
			unsigned long long input;
			consoleHandler.cursorPos = COORD{0, height - 1};
			consoleHandler.clearChars(width - 1);
			std::cin >> input;
			LARGE_INTEGER lI = {ovlpt.Offset, ovlpt.OffsetHigh};
			lI.QuadPart = input;
			ovlpt.Offset = lI.LowPart;
			ovlpt.OffsetHigh = lI.HighPart;
			consoleHandler.cursorPos = COORD{0, 0};
			consoleHandler.cursorPos = COORD{0, 1};
			cursorPos = 0;
		}
			break;
		case 'e':
		{
			// End file on cursor

			LARGE_INTEGER wholeWriteOffset = { ovlpt.Offset, ovlpt.OffsetHigh };
			wholeWriteOffset.QuadPart += cursorPos;

			if (SetFilePointerEx(hFile, wholeWriteOffset, NULL, FILE_BEGIN))
			{
				SetEndOfFile(hFile);
				GetFileSizeEx(hFile, &fileSize);
				readValues();
				drawValues();
				if ((size_t)fileSize.QuadPart > (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32)))
					if (((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)) < ((height - 2) * (width / 4)))
					{
						consoleHandler.clearChars(((height - 2) * width) - 4 * ((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)));
					}
				drawCursor();
			}
		}
		break;
		case '-':
			// decrement value at cursor position
		{
			BYTE byteToWrite = fileBytesDisplaying[cursorPos] - 1;
			writeOvlpt.Offset = ovlpt.Offset;
			writeOvlpt.OffsetHigh = ovlpt.OffsetHigh;
			if (writeOvlpt.Offset + cursorPos < writeOvlpt.Offset)
			{
				if (writeOvlpt.OffsetHigh != 0xffffffff)
				{
					writeOvlpt.Offset += cursorPos;
					++writeOvlpt.OffsetHigh;
				}
			}
			else writeOvlpt.Offset += cursorPos;
			WriteFile(hFile, (LPCVOID)&byteToWrite, 1, &bytesWritten, &writeOvlpt);

			LARGE_INTEGER wholeWriteOffset = { writeOvlpt.Offset, writeOvlpt.OffsetHigh };
			if ((size_t)wholeWriteOffset.QuadPart >= (size_t)fileSize.QuadPart)
			{
				GetFileSizeEx(hFile, &fileSize);
			}

			readValues();
			drawValues();
			drawCursor();
			drawInfo();
		}
			break;
		case '=':
			// + increment value
		{
			BYTE byteToWrite = fileBytesDisplaying[cursorPos] + 1;
			writeOvlpt.Offset = ovlpt.Offset;
			writeOvlpt.OffsetHigh = ovlpt.OffsetHigh;
			if (writeOvlpt.Offset + cursorPos < writeOvlpt.Offset)
			{
				if (writeOvlpt.OffsetHigh != 0xffffffff)
				{
					writeOvlpt.Offset += cursorPos;
					++writeOvlpt.OffsetHigh;
				}
			}
			else writeOvlpt.Offset += cursorPos;
			WriteFile(hFile, (LPCVOID)&byteToWrite, 1, &bytesWritten, &writeOvlpt);

			LARGE_INTEGER wholeWriteOffset = { writeOvlpt.Offset, writeOvlpt.OffsetHigh };
			if ((size_t)wholeWriteOffset.QuadPart >= (size_t)fileSize.QuadPart)
			{
				GetFileSizeEx(hFile, &fileSize);
			}

			readValues();
			drawValues();
			drawCursor();
			drawInfo();
		}
			break;
		case ' ':
			// Select cursor location and start entering the numerical value to insert
		{
			consoleHandler.cursorPos = COORD{0, height - 1};
			consoleHandler.clearChars(width - 1);
			consoleHandler.cursorPos = COORD{ 0, height - 1 };
			consoleHandler << L"Enter Decimal Value: ";
			unsigned int byteToWrite;
			std::cin >> byteToWrite;
			consoleHandler.cursorPos = COORD{ 0, height - 1 };
			consoleHandler.clearChars(width);
			consoleHandler.cursorPos = COORD{0, 0};
			//consoleHandler << L"My Hex Editor!\n";

			writeOvlpt.Offset = ovlpt.Offset;
			writeOvlpt.OffsetHigh = ovlpt.OffsetHigh;
			if (writeOvlpt.Offset + cursorPos < writeOvlpt.Offset)
			{
				if (writeOvlpt.OffsetHigh != 0xffffffff)
				{
					writeOvlpt.Offset += cursorPos;
					++writeOvlpt.OffsetHigh;
				}
			}
			else writeOvlpt.Offset += cursorPos;
			WriteFile(hFile, (LPCVOID)&byteToWrite, 1, &bytesWritten, &writeOvlpt);
			GetOverlappedResult(hFile, &writeOvlpt, &bytesWritten, TRUE);

			LARGE_INTEGER wholeWriteOffset = { writeOvlpt.Offset, writeOvlpt.OffsetHigh };
			if ((size_t)wholeWriteOffset.QuadPart >= (size_t)fileSize.QuadPart)
			{
				GetFileSizeEx(hFile, &fileSize);
			}


			readValues();
			drawValues();
			drawCursor();
			drawInfo();
		}
			break;
		case 'n':
			// shift left
			if (ovlpt.Offset != 0) // ovlpt.Offset is not marked as unsigned even though it is. So this is for if it's greater than zero
				ovlpt.Offset--;
			else
				if (ovlpt.OffsetHigh != 0)
				{
					--ovlpt.OffsetHigh;
					ovlpt.Offset = ~0;  // inverting (using ~) zero is the same as subtracting 1 from 0 (but might be faster (also I thought of this method first for some reason))
				}
				else
					break;
			consoleHandler.cursorPos = COORD{0, height - 1};
			consoleHandler.clearChars(width - 1);
			readValues();
			drawValues();
			drawCursor();
			drawInfo();
			break;
		case 'm':
			// shift right
			if ((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32))
			{
				if (~ovlpt.Offset != 0)
					ovlpt.Offset++;
				else
					if (~ovlpt.OffsetHigh != 0)
					{
						++ovlpt.OffsetHigh;
						ovlpt.Offset = 0;
					}
					else break;
				readValues();
				drawValues();
				drawCursor();
				if ((size_t)fileSize.QuadPart > (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32)))
					if (((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)) < ((height - 2) * (width / 4)))
					{
						consoleHandler.clearChars(((height - 2) * width) - 4 * ((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)));
					}
				drawCursor();
				drawInfo();
			}
			break;
		case ',':
			// less than (tecnically comma) scrolls up a lot
			if ((unsigned int)ovlpt.Offset >= (scrollAmount / 4))
				ovlpt.Offset -= (scrollAmount / 4);
			else
				if (ovlpt.OffsetHigh != 0)
				{
					--ovlpt.OffsetHigh;
					ovlpt.Offset -= (scrollAmount / 4);
				}
				else
					break;
			consoleHandler.cursorPos = COORD{ 0, height - 1 };
			consoleHandler.clearChars(width - 1);
			readValues();
			drawValues();
			drawCursor();
			drawInfo();
			break;
		case '.':
			// greater than (period) scrolls down a lot
			if ((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32) > (scrollAmount / 4))
			{
				if ((unsigned int)(ovlpt.Offset + (scrollAmount / 4)) >= (unsigned int)ovlpt.Offset) // Would reqire width to be at least 1 over the 32-bit integer limit for this to not work right
					ovlpt.Offset += (scrollAmount / 4);
				else
					if (~ovlpt.OffsetHigh != 0)
					{
						++ovlpt.OffsetHigh;
						ovlpt.Offset += (scrollAmount / 4);
					}
					else break;
				readValues();
				drawValues();
				drawCursor();
				if ((size_t)fileSize.QuadPart > (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32)))
					if (((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)) < ((height - 2) * (width / 4)))
					{
						consoleHandler.clearChars((((height - 2) * width) - 4 * (fileSize.QuadPart - ovlpt.Offset - (DWORDExpand(ovlpt.OffsetHigh) << 32))));
					}
				drawCursor();
				drawInfo();
			}
			break;
		case 224:
			typedInput = _getch();
			switch (typedInput)
			{
			case 75:
				// left cursor key pressed
				if (cursorPos)
				{
					--cursorPos;
				}
				else if (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32))
				{
					if (ovlpt.Offset == 0)
					{
						if (ovlpt.OffsetHigh)
						{
							--ovlpt.OffsetHigh;
							--ovlpt.Offset;
						}
						else continue;
					}
					else
						--ovlpt.Offset;
					readValues();
					
				}
				drawValues();
				drawCursor();
				drawInfo();
				break;
			case 72:
				// up
				if ((cursorPos - (long long)(width/4))  >= 0)
				{
					cursorPos = cursorPos.pos - (width/4);
				}
				else
				{
					if (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32))
					{
						if ((long long)(unsigned int)ovlpt.Offset - (long long)(unsigned int)(width / 4) < 0)
						{
							if ((unsigned int)ovlpt.OffsetHigh)
							{
								ovlpt.Offset -= (width / 4);
								--ovlpt.OffsetHigh;
							}
							else
							{
								ovlpt.Offset = 0;
								cursorPos = 0;
							}
						}
						else
						{
							ovlpt.Offset -= (width / 4);
						}
						readValues();
					}
				}
				drawValues();

				drawCursor();
				drawInfo();
				break;
			case 77:
				// right

				if (cursorPos < width * (height - 2) - 1)
				{
					++cursorPos;
					consoleHandler.cursorPos = COORD{ 0, height - 1 };
					consoleHandler.clearChars(width - 1);
				}
				else
				{
					if (ovlpt.Offset == 0xffffffff)
						++ovlpt.OffsetHigh;
					++ovlpt.Offset; // intentionaly separate from last if

					readValues();
					/* solution that does not wrap around at the 64-bit integer limit, but it has more branching and I don't think files can even get that large, so it's excluded
					++ovlpt.Offset;
					if (ovlpt.Offset == 0)
						if (ovlpt.OffsetHigh != 0xffffffff)
							++ovlpt.OffsetHigh;
						else
							--ovlpt.Offset;
					*/

						
					drawValues();
					drawCursor();
					if ((size_t)fileSize.QuadPart > (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32)))
						if (((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)) < ((height - 2) * (width / 4)))
						{
							consoleHandler.clearChars((((height - 2) * width) - 4 * ((size_t)fileSize.QuadPart - (unsigned int)ovlpt.Offset - (DWORDExpand(ovlpt.OffsetHigh) << 32))));
						}
					}
				drawValues();
				drawCursor();
				drawInfo();
				break;
			case 80:
				// down
			{
				size_t totalOffset = DWORDExpand(cursorPos) + DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32);
				
				if (cursorPos + (width / 4) < (width / 4) * (height - 2))
				{
					cursorPos = cursorPos + (width / 4);
				}
				else
				{
					if ((long long)(unsigned int)ovlpt.Offset + (long long)(unsigned int)(width / 4) > 0xffffffff)
					{
						if ((unsigned int)ovlpt.OffsetHigh != 0xffffffff)
						{
							ovlpt.Offset += (width / 4);
							++ovlpt.OffsetHigh;
						}
						else
						{
							ovlpt.Offset = 0xffffffff;
							cursorPos = (width / 4) * height - 1;
						}
					}
					else
					{
						ovlpt.Offset += (width / 4);
					}
					readValues();
					consoleHandler.cursorPos = COORD{ 0, height - 1 };
					consoleHandler.clearChars(width - 1);
					drawValues();
					drawCursor();
					if ((size_t)fileSize.QuadPart > (DWORDExpand(ovlpt.Offset) + (DWORDExpand(ovlpt.OffsetHigh) << 32)))
						if (((size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)) < ((height - 2) * (width / 4)))
						{
							consoleHandler.clearChars((((height - 2) * width) - 4 * ((size_t)fileSize.QuadPart - (unsigned int)ovlpt.Offset - (DWORDExpand(ovlpt.OffsetHigh) << 32))));
						}
				}
			}
				drawValues();
				drawCursor();
				drawInfo();
				break;
			default:
				break;
			}
			break;
		default:
			std::cout << typedInput << ' ';
		}
		
	}

	if (hFile)
	{
		CloseHandle(hFile);
	}
	
}

void readValues()
{
	ReadFile(hFile, &fileBytesDisplaying, min(bufferSize, (unsigned long long)fileSize.QuadPart - (unsigned long long)ovlpt.Offset - ((unsigned long long)ovlpt.OffsetHigh << 32)), &bytesRead, &ovlpt); // Look into if the warning is correct (I think GetOverlapped result lets me know of the same error)
	bResult = GetOverlappedResult(hFile, &ovlpt, &bytesRead, TRUE);
	if (!bResult)
	{
		dwError = GetLastError();
		
		switch (dwError)
		{
			case ERROR_HANDLE_EOF:
			{
				consoleHandler << L"EOF reached";
				// Use GetFileSize instead of this method
			}
			/*
			case ERROR_IO_PENDING:
			{
				
				BOOL bPending = TRUE;
				
				while (bPending)
				{
					// Maybe allow file scrolling while it's pending (not going to add it yet though)
					bResult = GetOverlappedResult(hFile, &ovlpt, &bytesRead, FALSE);
				}
				
			}
			*/
		}
	}
	ResetEvent(ovlpt.hEvent);


}

void drawValues()
{
	consoleHandler.cursorPos = COORD{ 0, 1 };

	for (size_t i = 0; i < min(bufferSize, (size_t)fileSize.QuadPart - DWORDExpand(ovlpt.Offset) - (DWORDExpand(ovlpt.OffsetHigh) << 32)); ++i)
	{
		consoleHandler << fileBytesDisplaying[i];
		
			consoleHandler << L" ";
	}
}


void drawCursor()
{
	COORD oldCursorPos = consoleHandler.cursorPos.coord;

	consoleHandler.cursorPos = COORD{((short)cursorPos % ((short)width / 4)) * 4, (short)cursorPos / ((short)width / 4) + 1};
	consoleHandler << L"@";
	consoleHandler.cursorPos += COORD{2, 0};
	consoleHandler << L"@";

	consoleHandler.cursorPos = oldCursorPos;
}

void drawInfo()
{
	LARGE_INTEGER wholeOffset = {ovlpt.Offset, ovlpt.OffsetHigh};
	wholeOffset.QuadPart += cursorPos;
	consoleHandler.cursorPos = COORD{0, height - 1};
	consoleHandler.clearChars(width - 1);
	std::cout << "Cursor Offset: " << wholeOffset.QuadPart << " Pos: " << wholeOffset.QuadPart + 1 << " File Size: " << fileSize.QuadPart << "   Value Dec: " << (int)fileBytesDisplaying[cursorPos.pos] << " ANSI Char: ";

	switch (fileBytesDisplaying[cursorPos.pos])
	{
	case 27:
		std::wcout << L"[Escape. Next char is not echoed]";
		break;
	case 13:
		std::wcout << L"\\r [Return]\r";
		break;
	case 9:
		std::wcout << L"	 [Tab]";
		break;
	case 7:
		std::wcout << L"  [Bell]";
		break;
	case 0:
		std::wcout << L"  [NULL]";
		break;
	case 32:
		std::wcout << L"  [Space]";
		break;
	case 8:
		std::wcout << L"\\b [Backspace]";
		break;
	case 10:
		std::wcout << L"\\n [NewLine]";
		break;
	default:
		std::wcout << (wchar_t)fileBytesDisplaying[cursorPos.pos];
		break;
	}
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
