#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <fstream>
#include <stdio.h>

#define MAX_PATH_LENGTH 4095

// variables
std::string scale = "minorHarm";  // major, minor or minorHarm, penta
int mode = 3;					 // cell choosing mode: 0 - alive, 1 - newborn, 2 - highest alive per row, 3 - highest newborn per row
const int size = 20;			  // the size of the board
const int step = 2;			   // change between rows
double speed = 0.3;			   // time between updates
int start = 3;					// starting octave midi note
const char* tone = "C";		   // starting note name (don't put a flat note, put a note with a # that's identical to it instead)
float volume = 50;				  // volume control

//controls
bool grid = true;	   // show grid
bool paused = true;	  // pause the simulation
bool allNames = false;   // show the name of every tile
bool playedNames = true; // show the name of played tiles
bool midiNames = false;  // show the midi note of every tile
bool drawGUI = false;	// show GUI

const int screenWidth = 800;   // width of the screen
const int screenHeight = 800;  // height of the screen

Color normCol = {0, 21, 41, 255};	   // color of a normal tile
Color filledCol = {48, 109, 142, 255};   // color of a filled tile
Color playedCol = {255, 140, 0, 255};   // color of a played tile
Color textCol = {0, 213, 247, 255};	 // color of the text on a tile
Color gridCol = {0, 255, 255, 255};	 // grid color

int startSave = start; // just in case
bool recording = false;
std::vector<float> audioRec; // recording audio

std::vector<int> notes;
std::vector<int> notesDur;

struct Note {
	float freq;
	int duration;
	float phase;
};

std::vector<Note> activeNotes;

void MyAudioCallback(void *buffer, unsigned int frames) {
	float *audioBuffer = (float *)buffer;
	
	for (unsigned int i = 0; i < frames; i++) {
		for (int j = 0; j < activeNotes.size(); j++) {
			float decayFactor = exp(-activeNotes[j].duration / 6000.0f);
			
			if (decayFactor > 0.01f) {
				float mono = sin(activeNotes[j].phase) * decayFactor * (volume / 300.0f); // Reduced volume
				audioBuffer[i*2] += mono;
				audioBuffer[i*2+1] += mono;
				
				activeNotes[j].phase += activeNotes[j].freq * 2.0f * PI / 44100.0f;
				
				if (activeNotes[j].phase > 2.0f * PI) {
					activeNotes[j].phase -= 2.0f * PI;
				}
				
				activeNotes[j].duration++;
			} else {
				activeNotes.erase(activeNotes.begin() + j);
				j--;
			}
		}
	}
	if (recording) {
		audioRec.insert(audioRec.end(), audioBuffer, audioBuffer + frames * 2);
	}
}

float noteToFreq(int note) {
	return 440.0 * pow(2.0, (note - 60)/12.0);
}

void playNote(int note) {
	float freq = noteToFreq(note);
	activeNotes.push_back({freq, 0, 0});
}

int major[] = {0, 2, 4, 5, 7, 9, 11};
int minor[] = {0, 2, 3, 5, 7, 8, 10};
int minorHarm[] = {0, 2, 3, 5, 7, 8, 11};
int penta[] = {0, 2, 5, 7, 9};

int map[size][size];

const char* noteNames[] = {"A", "A#", "H", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};

int gen = 0;

int getMap(int i, int j, const int scale[], int scSize) {
	return (scale[(i + (size-j-1)*step) % scSize] + ((int)(i + (size-j-1)*step) / scSize) * 12) + start;
}

bool played[size][size];
bool filled[size][size];
bool nextFilled[size][size];

void initMap() {
	start = startSave;
	
	int nsize = sizeof(noteNames) / sizeof(noteNames[0]);
	
	const char** ptr = std::find(noteNames, noteNames + nsize, tone);
	
	if (ptr != noteNames + nsize) {
		start += std::distance(noteNames, ptr) - 3;
	}
	
	const int* selectedScale = (scale == "major" ? major : (scale == "minor" ? minor : (scale == "minorHarm" ? minorHarm : penta)));
	const int scaleSize = scale == "penta" ? 5 : 7;
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			map[i][j] = getMap(i, j, selectedScale, scaleSize);
		}
	}
}

void initAll() {
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			played[i][j] = false;
			filled[i][j] = false;
			nextFilled[i][j] = false;
		}
	}
	initMap();
}

void update() {
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			int count = 0;
			for (int i1 = -1; i1 <= 1; i1++) {
				for (int j1 = -1; j1 <= 1; j1++) {
					if (i1 != 0 || j1 != 0) {
						int ineig = (i + i1 + size) % size;
						int jneig = (j + j1 + size) % size;
						if (filled[ineig][jneig]) count++;
					}
				}
			}
			if (filled[i][j]) {
				if (count < 2 || count > 3) nextFilled[i][j] = false;
				else nextFilled[i][j] = true;
			} else {
				if (count == 3) nextFilled[i][j] = true;
				else nextFilled[i][j] = false;
			}
		}
	}
	int rightmost[size];
	for (int i = 0; i < size; i++) rightmost[i] = -1;
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			played[i][j] = false;
			if ((mode != 2 && mode != 0 ? (nextFilled[i][j] == 1 && filled[i][j] == 0) : nextFilled[i][j])) {
				if (mode != 1 && mode != 0 && i > rightmost[j]) rightmost[j] = i;
				else if (mode == 1 || mode == 0) {
					playNote(map[i][j]);
					played[i][j] = true;
				}
			}
			filled[i][j] = nextFilled[i][j];
		}
	}
	if (mode != 1 && mode != 0) {
		for (int i = 0; i < size; i++) {
			if (rightmost[i] != -1) {
				playNote(map[rightmost[i]][i]);
				played[rightmost[i]][i] = true;
			}
		}
	}
}

float tileWidth = screenWidth/(float)size;
float tileHeight = screenHeight/(float)size;

void drawOne(int x, int y) {
	float posX = x*tileWidth;
	float posY = y*tileHeight;
	float textSize = 800 / size / 2;
	if (grid) {
		if (played[x][y]) {
			DrawRectangle(posX+1, posY+1, tileWidth-1, tileHeight-1, playedCol);
			if (playedNames) DrawText(noteNames[map[x][y]%12], posX+textSize/2, posY+textSize/2, textSize, textCol);
		}
		else if (filled[x][y]) DrawRectangle(posX+1, posY+1, tileWidth-1, tileHeight-1, filledCol);
		else DrawRectangle(posX+1, posY+1, tileWidth-1, tileHeight-1, normCol);
	} else {
		if (played[x][y]) {
			DrawRectangle(posX, posY, tileWidth, tileHeight, playedCol);
			if (playedNames) DrawText(noteNames[map[x][y]%12], posX+textSize/2, posY+textSize/2, textSize, textCol);
		}
		else if (filled[x][y]) DrawRectangle(posX, posY, tileWidth, tileHeight, filledCol);
		else DrawRectangle(posX, posY, tileWidth, tileHeight, normCol);
	}
	if (allNames) {
		DrawText(noteNames[map[x][y]%12], posX+textSize/2, posY+textSize/2, textSize, textCol);
	}
	if (midiNames) {
		DrawText(std::to_string(map[x][y]).c_str(), posX+textSize+textSize/4, posY+textSize+textSize/4, textSize/4, textCol);
	}
}

void loadState(char* filePath) {
	std::ifstream file(filePath);
	int i = 0;
	int j = 0;
	int starti = 0;
	char ch;
	char num[255];
	while (file.get(ch)) {
		if (ch == ',') break;
		num[i] = ch;
		i++;
	}
	starti = std::stoi(num);
	while (file.get(ch)) {
		if (ch == '\n') break;
		num[j] = ch;
		j++;
	}
	j = size-std::stoi(num);
	i = starti;
	while (file.get(ch)) {
		if (ch != '\n') {
			if (i < size) filled[i][j] = ch == 'O';
			i++;
		} else {
			i = starti;
			j++;
			j = (j+size) % size;
		}
	}
	gen = 0;
}

void drawAll() {
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			drawOne(i, j);
		}
	}
}


void SaveAudioBufferToFile(const std::vector<float>& buffer, const char* filename) {
	Wave recordedWave = {
		.frameCount = static_cast<unsigned int>(audioRec.size() / 2), // Stereo: 2 samples per frame
		.sampleRate = 44100,
		.sampleSize = 32,
		.channels = 2,
		.data = audioRec.data() // Pointer to our recorded samples
	};
			
	if (ExportWave(recordedWave, "recording.wav")) {
		TraceLog(LOG_INFO, "Saved as recording.wav!");
	} else {
		TraceLog(LOG_ERROR, "Failed to save recording!");
	}
	UnloadWave(recordedWave);
}

void DrawGUI() {
	if (!drawGUI) return;

	float speedValue = (float)speed;
	static int modeSel = 3;
	static int modeEditMode = false;
	static int scaleIndex = 2; // minorHarm by default
	static int scaleEditMode = false;
	static int noteEditMode = false;
	
	// Checkbox states - initialized to current values
	bool gridState = grid;
	bool playedNamesState = playedNames;
	bool allNamesState = allNames;
	bool midiNamesState = midiNames;
	bool recState = recording;

	// Panel dimensions
	float panelWidth = screenWidth * 0.4f;
	float panelX = screenWidth - panelWidth;
	
	// Draw panel background
	DrawRectangle(panelX, 0, panelWidth, screenHeight, Color{50, 50, 60, 230});
	
	// Set GUI style
	GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
	GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(GRAY));
	
	float x = panelX + 10;
	float y = 10;
	float elementWidth = panelWidth - 20;
	
	// Title
	GuiLabel((Rectangle){x, y, elementWidth, 20}, "Game of Life Music");
	y += 30;
	
	if (GuiButton((Rectangle){x, y, elementWidth, 30}, paused ? "Play (Space)" : "Pause (Space)")) {
		paused = !paused;
	}
	y += 40;
	
	if (GuiButton((Rectangle){x, y, elementWidth, 30}, recording ? "Stop recording (Space)" : "Start recording (Space)")) {
		recording = !recording;
		if (!audioRec.empty()) SaveAudioBufferToFile(audioRec, "recorded_audio.wav");
		audioRec.clear();
	}
	y += 40;
	
	if (GuiButton((Rectangle){x, y, elementWidth, 30}, "Clear (Del)")) {
		for (int i = 0; i < size; i++) {
			for (int j = 0; j < size; j++) {
				filled[i][j] = 0;
				played[i][j] = 0;
			}
		}
		gen = 0;
	}
	y += 35;
	
	GuiSlider((Rectangle){x+50, y, elementWidth-70, 20}, "Volume", TextFormat("%2.2f", volume), &volume, 0, 100);
	y += 35;
	
	GuiLabel((Rectangle){x, y, elementWidth, 20}, "Mode:");
	y += 25;
	if (GuiDropdownBox((Rectangle){x, y, elementWidth, 25}, "Alive;Newborn;Highest Alive;Highest Newborn", &modeSel, modeEditMode)) {
		modeEditMode = !modeEditMode;
		mode = modeSel;
	}
	if (modeEditMode) y += 100;
	y += 35;
	
	GuiLabel((Rectangle){x, y, elementWidth, 20}, "Scale:");
	y += 25;
	if (GuiDropdownBox((Rectangle){x, y, elementWidth, 25}, "Major;Minor;Minor Harmonic;Pentatonic", &scaleIndex, scaleEditMode)) {
		scaleEditMode = !scaleEditMode;
		switch(scaleIndex) {
			case 0: scale = "major"; break;
			case 1: scale = "minor"; break;
			case 2: scale = "minorHarm"; break;
			case 3: scale = "penta"; break;
		}
		initMap();
	}
	if (scaleEditMode) y += 100;
	y += 35;
	
	GuiLabel((Rectangle){x, y, elementWidth, 20}, "Root Note:");
	y += 25;

	static int rootNoteIndex = 3; // Default to C
	const char* noteOptions = "A;A#;H;C;C#;D;D#;E;F;F#;G;G#";
	if (GuiDropdownBox((Rectangle){x, y, elementWidth, 25}, noteOptions, &rootNoteIndex, noteEditMode)) {
		noteEditMode = !noteEditMode;
		tone = noteNames[rootNoteIndex];
		initMap(); // Rebuild all notes
	}
	if (noteEditMode) y += 320;
	y += 30;
	
	// Display options
	GuiLabel((Rectangle){x, y, elementWidth, 20}, "Display Options:");
	y += 25;
	
	GuiCheckBox((Rectangle){x, y, 20, 20}, "Show Grid", &gridState);
	grid = gridState == 1;
	y += 30;
	
	GuiCheckBox((Rectangle){x, y, 20, 20}, "Show Played Names (p)", &playedNamesState);
	playedNames = playedNamesState == 1;
	y += 30;
	
	GuiCheckBox((Rectangle){x, y, 20, 20}, "Show All Names (a)", &allNamesState);
	allNames = allNamesState == 1;
	y += 30;
	
	GuiCheckBox((Rectangle){x, y, 20, 20}, "Show MIDI Numbers (m)", &midiNamesState);
	midiNames = midiNamesState == 1;
	y += 120;
	
	// Instructions
	GuiSetStyle(DEFAULT, TEXT_SIZE, 15);
	GuiLabel((Rectangle){x, y, elementWidth, 80}, 
		"Controls:\n"
		"- Left click: Add cell\n"
		"- Right click: Remove cell\n"
		"- Space: Play/Pause\n"
		"- G: Toggle GUI\n"
		"- Del: Clear all\n"
		"- You can drop a file with a\ngame state and it will display here,\nthey must be in life lexicon form\n"
		"OR also you can drop a theme and\nif you like it you can change\nthe theme.rgs file to it");
}

int main() {
	InitWindow(screenWidth+1, screenHeight+1, "Game of life -> music visualization");
	InitAudioDevice();
	
	SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
	
	AudioStream stream = LoadAudioStream(44100, 32, 2);
	SetAudioStreamCallback(stream, MyAudioCallback);
	
	PlayAudioStream(stream);
	
	initAll();
	
	GuiLoadStyle("theme.rgs");
	
	start *= 12;
	double timer = GetTime();
	
	bool dropped = false;
	char filePath[MAX_PATH_LENGTH];
	
	while (!WindowShouldClose()) {
		if (GetTime() - timer > speed && !paused) {
			timer = GetTime();
			update();
			gen++;
		}
		
		if (IsMouseButtonDown(0) && !(drawGUI && GetMouseX() > screenWidth * 0.6f)) {
			filled[((int)(GetMouseX()/tileWidth)+size)%size][((int)(GetMouseY()/tileHeight)+size)%size] = 1;
			played[((int)(GetMouseX()/tileWidth)+size)%size][((int)(GetMouseY()/tileHeight)+size)%size] = 0;
		}
		
		if (IsMouseButtonDown(1) && !(drawGUI && GetMouseX() > screenWidth * 0.6f)) {
			filled[((int)(GetMouseX()/tileWidth)+size)%size][((int)(GetMouseY()/tileHeight)+size)%size] = 0;
			played[((int)(GetMouseX()/tileWidth)+size)%size][((int)(GetMouseY()/tileHeight)+size)%size] = 0;
		}
		
		if (IsKeyPressed(32)) {
			paused = !paused;
		}
		
		if (IsKeyPressed(77)) {
			midiNames = !midiNames;
		}
		
		if (IsKeyPressed(80)) {
			playedNames = !playedNames;
		}
		
		if (IsKeyPressed(65)) {
			allNames = !allNames;
		}
		
		if (IsKeyPressed(71)) {
			drawGUI = !drawGUI;
		}
		
		if (IsKeyPressed(KEY_DELETE)) {
			for (int i = 0; i < size; i++) {
				for (int j = 0; j < size; j++) {
					filled[i][j] = 0;
					played[i][j] = 0;
				}
			}
			gen = 0;
		}
		
		if (IsKeyPressed(KEY_LEFT)) {
			// Shift grid left
			for (int j = 0; j < size; j++) {
				bool temp = filled[0][j];
				for (int i = 0; i < size-1; i++) {
					filled[i][j] = filled[i+1][j];
				}
				filled[size-1][j] = temp;
			}
		}

		if (IsKeyPressed(KEY_RIGHT)) {
			// Shift grid right
			for (int j = 0; j < size; j++) {
				bool temp = filled[size-1][j];
				for (int i = size-1; i > 0; i--) {
					filled[i][j] = filled[i-1][j];
				}
				filled[0][j] = temp;
			}
		}

		if (IsKeyPressed(KEY_UP)) {
			// Shift grid up
			for (int i = 0; i < size; i++) {
				bool temp = filled[i][0];
				for (int j = 0; j < size-1; j++) {
					filled[i][j] = filled[i][j+1];
				}
				filled[i][size-1] = temp;
			}
		}

		if (IsKeyPressed(KEY_DOWN)) {
			// Shift grid down
			for (int i = 0; i < size; i++) {
				bool temp = filled[i][size-1];
				for (int j = size-1; j > 0; j--) {
					filled[i][j] = filled[i][j-1];
				}
				filled[i][0] = temp;
			}
		}
		
		if (IsFileDropped()) {
			FilePathList droppedFiles = LoadDroppedFiles();
			if (droppedFiles.count > 0) {
				if ((droppedFiles.count > 0) && IsFileExtension(droppedFiles.paths[0], ".rgs")) GuiLoadStyle(droppedFiles.paths[0]);
				else {
					strncpy(filePath, droppedFiles.paths[0], sizeof(filePath) - 1);
					filePath[sizeof(filePath) - 1] = '\0';
					dropped = true;
				}
			}
			UnloadDroppedFiles(droppedFiles);
		}
		
		if (dropped) {
			loadState(filePath);
			dropped = false;
		}
		
		BeginDrawing();
		ClearBackground(gridCol);
		
		drawAll();
		
		if (drawGUI) DrawGUI();
		
		if (paused) DrawText("paused", 100, -5, 50, textCol);
		DrawText(TextFormat("Currect generation: %d", gen), 10, screenHeight-30, 30, textCol);
		DrawFPS(10, 10);
		EndDrawing();
	}
	if (recording) {
		SaveAudioBufferToFile(audioRec, "autosave.wav");
		audioRec.clear();
	}
	UnloadAudioStream(stream);
	CloseAudioDevice();
	CloseWindow();

	return 0;
}

// g++ -o main main.cpp -I"C:\raylib\include" -L"C:\raylib\lib" -I"C:\raygui\src" -lraylib -lopengl32 -lgdi32 -lwinmm -static